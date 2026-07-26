#include "render/renderer/donut/RendererDonut.h"

#include <algorithm>
#include <memory>
#include <vector>

#include <DirectXCollision.h>

#include "dx_util/ResourceUtils.h"
#include "render/pass/donut/PassDonutGBuffer.h"
#include "scene/builder/cpu/SceneCPUBuilder.h"
#include "scene/builder/cpu/SceneCPUDrawBuilder.h"
#include "scene/builder/gpu/DonutSceneGPUBuilder.h"
#include "scene/builder/source/SceneSourceLoader.h"
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

        scene::SceneSourceData source =
            scene::SceneSourceLoader::load(program_argument_);
        scene_cpu_ = std::make_unique<scene::SceneCPUData>(
            scene::SceneCPUBuilder::build(source));

        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> used_upload_heaps;
        util::Utils::throw_if_failed(command_list_->Reset(
            command_allocator_[frame_index_].Get(), nullptr));

        scene_gpu_ = std::make_unique<scene::DonutSceneGPUData>(
            scene::DonutSceneGPUBuilder::build(
                *scene_cpu_,
                device_.Get(),
                command_list_.Get(),
                used_upload_heaps,
                program_argument_.to_load_texture));

        to_profile_index_count_ = true;
        profile_index_count_ = static_cast<double>(
            scene::SceneCPUDrawBuilder::count_indices(*scene_cpu_));

        util::Utils::throw_if_failed(command_list_->Close(),
            "close command list on Donut scene resource creation");
        graphics_queue_.execute(command_list_.Get());
        graphics_queue_.wait_idle();

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

            scene::SceneCPUDrawBuilder::build_visible(
                *scene_cpu_,
                world_frustum);
            scene::DonutSceneGPUBuilder::rebuild_draws(
                *scene_cpu_,
                *scene_gpu_);

            profile_index_count_ = static_cast<double>(
                scene::SceneCPUDrawBuilder::count_indices(
                    *scene_cpu_));
        }

        this->render_prepare_donut_();
    }

}
