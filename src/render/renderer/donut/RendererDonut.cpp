#include "render/renderer/donut/RendererDonut.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include <DirectXCollision.h>

#include "dx_util/ResourceUtils.h"
#include "render/pass/donut/PassDonutGBuffer.h"
#include "scene/donut/DonutSceneAssimpImporter.h"
#include "scene/donut/DonutSceneResourceBuilder.h"
#include "util/Logger.h"
#include "util/RenderConstants.h"
#include "util/Utils.h"

namespace rndr {

    RendererDonut::~RendererDonut() {
        compute_queue_.wait_idle();
    }


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

        compute_queue_.init(device_.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE);
        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
            util::Utils::throw_if_failed(device_->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_COMPUTE,
                IID_PPV_ARGS(compute_command_allocator_[i].ReleaseAndGetAddressOf())),
                "create Donut compute command allocator");
        }

        util::Utils::throw_if_failed(device_->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_COMPUTE,
            compute_command_allocator_[0].Get(),
            nullptr,
            IID_PPV_ARGS(compute_command_list_.ReleaseAndGetAddressOf())),
            "create Donut compute command list");

        util::Utils::throw_if_failed(
            compute_command_list_->Close(),
            "close Donut compute command list");

        scene_cpu_ = scene::donut::DonutSceneAssimpImporter::load(
            program_argument_.scene_path);
        util::Logger::g_logger.assert_with_log(
            scene_cpu_ && scene_cpu_->loaded,
            scene_cpu_
            ? scene_cpu_->error_message.c_str()
            : "Donut scene import failed");

        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> used_upload_heaps;
        util::Utils::throw_if_failed(command_list_->Reset(
            command_allocator_[frame_index_].Get(), nullptr));

        scene_gpu_ = scene::donut::DonutSceneResourceBuilder::build(
            *scene_cpu_,
            device_.Get(),
            command_list_.Get(),
            used_upload_heaps,
            program_argument_.to_load_texture);

        to_profile_index_count_ = true;
        profile_index_count_ = 0.0;
        for (const scene::DonutSceneDataGPU::Draw& draw : scene_gpu_->draws) {
            profile_index_count_ +=
                static_cast<double>(draw.index_count) *
                static_cast<double>(draw.instance_count);
        }

        util::Utils::throw_if_failed(command_list_->Close(),
            "close command list on Donut scene resource creation");
        graphics_queue_.execute(command_list_.Get());
        graphics_queue_.wait_idle();

        const size_t render_instance_upload_size =
            static_cast<size_t>(scene_gpu_->render_instance_capacity) *
            sizeof(scene::DonutSceneDataGPU::InstanceData);
        for (Microsoft::WRL::ComPtr<ID3D12Resource>& upload :
            render_instance_upload_buffers_) {
            upload = dxutl::create_buffer(
                device_.Get(),
                render_instance_upload_size,
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_STATE_GENERIC_READ);
        }

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

            scene_cpu_->build_visible_draws(visible_draws_, &world_frustum);
            scene_cpu_->sort_visible_draws(visible_draws_);
            scene::donut::DonutSceneResourceBuilder::rebuild_draw_stream(
                *scene_cpu_,
                visible_draws_,
                *scene_gpu_);

            profile_index_count_ = 0.0;
            for (const scene::DonutSceneDataGPU::Draw& draw : scene_gpu_->draws) {
                profile_index_count_ +=
                    static_cast<double>(draw.index_count) *
                    static_cast<double>(draw.instance_count);
            }

            const size_t upload_size =
                scene_gpu_->render_instance_data.size() *
                sizeof(scene::DonutSceneDataGPU::InstanceData);
            render_instance_upload_sizes_[frame_index_] = upload_size;
            if (upload_size != 0) {
                util::Logger::g_logger.assert_with_log(
                    scene_gpu_->render_instance_data.size() <=
                    scene_gpu_->render_instance_capacity,
                    "Donut visible draw stream exceeds render instance buffer capacity");
                dxutl::copy_to_upload_buffer(
                    render_instance_upload_buffers_[frame_index_].Get(),
                    scene_gpu_->render_instance_data.data(),
                    upload_size);
            }
        }

        render_prepare_donut_();
    }

    void RendererDonut::record_render_instance_upload_(
        ID3D12GraphicsCommandList* command_list) {

        if (!program_argument_.use_vfc) {
            return;
        }

        const size_t upload_size = render_instance_upload_sizes_[frame_index_];
        if (upload_size == 0) {
            return;
        }

        ID3D12Resource* render_instance_buffer =
            scene_gpu_->render_instance_buffer.Get();
        dxutl::transition_resource(
            command_list,
            render_instance_buffer,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_COPY_DEST);
        command_list->CopyBufferRegion(
            render_instance_buffer,
            0,
            render_instance_upload_buffers_[frame_index_].Get(),
            0,
            upload_size);
        dxutl::transition_resource(
            command_list,
            render_instance_buffer,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    }

}
