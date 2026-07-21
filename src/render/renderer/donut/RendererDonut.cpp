#include "render/renderer/donut/RendererDonut.h"

#include <vector>

#include "dx_util/ResourceUtils.h"
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

        resource_manager_shader_.init(
            device_.Get(),
            static_cast<UINT>(eng::ResourceManagerShader::EnumDescPos::COUNT));
        resource_manager_frame_.init(device_.Get());
        resource_manager_sampler_.init(device_.Get());

        compute_queue_.init(device_.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE);
        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
            Utils::throw_if_failed(device_->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_COMPUTE,
                IID_PPV_ARGS(compute_command_allocator_[i].ReleaseAndGetAddressOf())),
                "create Donut compute command allocator");
        }

        Utils::throw_if_failed(device_->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_COMPUTE,
            compute_command_allocator_[0].Get(),
            nullptr,
            IID_PPV_ARGS(compute_command_list_.ReleaseAndGetAddressOf())),
            "create Donut compute command list");

        Utils::throw_if_failed(
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
        Utils::throw_if_failed(command_list_->Reset(
            command_allocator_[frame_index_].Get(), nullptr));

        scene_gpu_ = scene::donut::DonutSceneResourceBuilder::build(
            *scene_cpu_,
            device_.Get(),
            command_list_.Get(),
            used_upload_heaps,
            program_argument_.to_load_texture);

        Utils::throw_if_failed(command_list_->Close(),
            "close command list on Donut scene resource creation");
        graphics_queue_.execute(command_list_.Get());
        graphics_queue_.wait_idle();

        this->init2_();
    }

    void RendererDonut::render_prepare_() {

    }

}
