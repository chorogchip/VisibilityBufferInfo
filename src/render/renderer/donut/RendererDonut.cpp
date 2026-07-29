#include "render/renderer/donut/RendererDonut.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include <DirectXCollision.h>

#include "dx_util/ResourceUtils.h"
#include "render/pass/donut/PassDonutGBuffer.h"
#include "scene/cache/SceneCPUCache.h"
#include "scene/builder/cpu/SceneCPUDrawStreamBuilder.h"
#include "scene/builder/gpu/DonutSceneGPUBuilder.h"
#include "scene/builder/gpu/JungleSceneGPUBuilder.h"
#include "scene/builder/source/SceneSourceFactory.h"
#include "util/Logger.h"
#include "util/RenderConstants.h"
#include "util/Utils.h"

namespace rndr {

    void RendererDonut::init1_() {
        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = util::RenderConstants::DONUT_DEPTH_DSV_FORMAT;
        clear_value.DepthStencil.Depth = 1.f;

        depth_stencil_buffer_.init(
            dxutl::create_texture2d(
                device_.Get(),
                width_,
                height_,
                util::RenderConstants::DONUT_DEPTH_RESOURCE_FORMAT,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
                &clear_value).Get(),
            D3D12_RESOURCE_STATE_DEPTH_WRITE);

        resource_manager_frame_.init(device_.Get());
        resource_manager_sampler_.init(device_.Get());

        scene_cpu_ = scene::load_or_build_scene_cpu(program_argument_);
        scene::SceneCPUDrawStreamBuilder::build_all(
            *scene_cpu_,
            draw_stream_);

        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> used_upload_heaps;
        util::Utils::throw_if_failed(command_list_->Reset(
            command_allocator_[frame_index_].Get(), nullptr));
        const auto flush_uploads = [&]() {
            if (used_upload_heaps.empty()) {
                return;
            }

            util::Utils::throw_if_failed(
                command_list_->Close(),
                "close command list on Donut scene upload flush");
            graphics_queue_.execute(command_list_.Get());
            graphics_queue_.wait_idle();
            used_upload_heaps.clear();
            util::Utils::throw_if_failed(
                command_allocator_[frame_index_]->Reset(),
                "reset command allocator on Donut scene upload flush");
            util::Utils::throw_if_failed(
                command_list_->Reset(
                    command_allocator_[frame_index_].Get(),
                    nullptr),
                "reset command list on Donut scene upload flush");
        };

        if (scene::SceneSourceFactory::uses_jungle_builder(
            program_argument_)) {
            scene_gpu_ = std::make_unique<scene::DonutSceneGPUData>(
                scene::JungleSceneGPUBuilder::build_donut(
                    *scene_cpu_,
                    device_.Get(),
                    command_list_.Get(),
                    used_upload_heaps,
                    flush_uploads,
                    program_argument_.to_load_texture));
        }
        else {
            scene_gpu_ = std::make_unique<scene::DonutSceneGPUData>(
                scene::DonutSceneGPUBuilder::build(
                    *scene_cpu_,
                    device_.Get(),
                    command_list_.Get(),
                    used_upload_heaps,
                    flush_uploads,
                    program_argument_.to_load_texture));
        }

        to_profile_index_count_ = true;
        profile_index_count_ = static_cast<double>(
            scene::SceneCPUDrawStreamBuilder::count_indices(draw_stream_));

        util::Utils::throw_if_failed(command_list_->Close(),
            "close command list on Donut scene resource creation");
        graphics_queue_.execute(command_list_.Get());
        graphics_queue_.wait_idle();

        this->create_draw_instance_id_upload_buffers();

        const UINT donut_texture_begin = static_cast<UINT>(
            eng::ResourceManagerShader::EnumDescPos::DONUT_MATERIAL_TEXTURE_BEGIN);
        const UINT donut_texture_count =
            static_cast<UINT>(scene_gpu_->material_data.size()) *
            PassDonutGBuffer::MATERIAL_TEXTURE_DESCRIPTOR_COUNT;
        const UINT shader_descriptor_count = (std::max)(
            static_cast<UINT>(eng::ResourceManagerShader::EnumDescPos::COUNT),
            donut_texture_begin + donut_texture_count);
        resource_manager_shader_.init(device_.Get(), shader_descriptor_count);

        this->init2_();
    }

    void RendererDonut::render_prepare_() {
        if (program_argument_.use_vfc) {
            DirectX::BoundingFrustum view_frustum;
            DirectX::BoundingFrustum::CreateFromMatrix(
                view_frustum,
                camera_.get_mat_proj(width_, height_));

            DirectX::BoundingFrustum world_frustum;
            view_frustum.Transform(
                world_frustum,
                DirectX::XMMatrixInverse(nullptr, camera_.get_mat_view()));

            scene::SceneCPUDrawStreamBuilder::build_visible(
                *scene_cpu_,
                draw_stream_,
                world_frustum);
            draw_stream_dirty_ = true;

            profile_index_count_ = static_cast<double>(
                scene::SceneCPUDrawStreamBuilder::count_indices(
                    draw_stream_));
        }

        this->render_prepare_donut_();
    }

    void RendererDonut::render_update_scene_resources_() {
        this->update_draw_instance_id_buffer();
    }

    void RendererDonut::create_draw_instance_id_upload_buffers() {
        util::Logger::g_logger.assert_with_log(
            scene_gpu_ != nullptr &&
            scene_gpu_->draw_instance_id_capacity > 0,
            "Donut draw-instance ID buffer capacity is invalid");

        const uint64_t byte_size =
            static_cast<uint64_t>(scene_gpu_->draw_instance_id_capacity) *
            sizeof(uint32_t);
        for (auto& upload : draw_instance_id_upload_buffers_) {
            upload = dxutl::create_buffer(
                device_.Get(),
                byte_size,
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_STATE_GENERIC_READ);
        }
    }

    void RendererDonut::update_draw_instance_id_buffer() {
        if (!draw_stream_dirty_)
            return;

        util::Logger::g_logger.assert_with_log(
            draw_stream_.draw_instance_ids_compacted.size() <=
            scene_gpu_->draw_instance_id_capacity,
            "Donut compacted draw-instance ID stream exceeds GPU capacity");

        const uint64_t byte_size =
            static_cast<uint64_t>(
                draw_stream_.draw_instance_ids_compacted.size()) *
            sizeof(uint32_t);
        if (byte_size > 0) {
            dxutl::copy_to_upload_buffer(
                draw_instance_id_upload_buffers_[frame_index_].Get(),
                draw_stream_.draw_instance_ids_compacted.data(),
                static_cast<size_t>(byte_size));
            scene_gpu_->draw_instance_id_buffer.transition(
                command_list_.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST);
            command_list_->CopyBufferRegion(
                scene_gpu_->draw_instance_id_buffer.get(),
                0,
                draw_instance_id_upload_buffers_[frame_index_].Get(),
                0,
                byte_size);
            scene_gpu_->draw_instance_id_buffer.transition(
                command_list_.Get(),
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        }

        draw_stream_dirty_ = false;
    }

}
