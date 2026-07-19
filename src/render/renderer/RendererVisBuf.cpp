#include "render/renderer/RendererVisBuf.h"

#include <d3d12.h>
#include <string>

#include "util/Utils.h"
#include "dx_util/ResourceUtils.h"
#include "engine/MaterialGPU.h"
#include "dx_util/ShaderUtils.h"
#include "dx_util/DescriptorUtils.h"
#include "render/VisBufResourceBuilder.h"

namespace rndr {

    void RendererVisBuf::make_programresult(util::ProgramResult& result) {
        result.renderer_name = "VisBuf";
        result.pass_name_0 = "visibility";
        result.pass_name_1 = "resolve";
        result.pass_name_3 = "total";
    }

    void RendererVisBuf::create_pass_resources() {
        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = DXGI_FORMAT_R32G32_UINT;
        clear_value.Color[0] = 0.0f;
        clear_value.Color[1] = 0.0f;
        clear_value.Color[2] = 0.0f;
        clear_value.Color[3] = 0.0f;

        vis_buffer_ = dxutl::create_texture2d(device_.Get(), width_, height_,
            DXGI_FORMAT_R32G32_UINT,
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            &clear_value);
    }

    void RendererVisBuf::render_() {
        Utils::throw_if_failed(command_allocator_[frame_index_]->Reset(), "reset command allocator");
        Utils::throw_if_failed(command_list_->Reset(command_allocator_[frame_index_].Get(), nullptr), "command list reset on render start");
        copy_camera_data();
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 0);
        pass_visibility_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 0);
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_resolve_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);
        Utils::throw_if_failed(command_list_->Close(), "command list close on frame end");
        ID3D12CommandList* lists[] = { command_list_.Get() };
        command_queue_->ExecuteCommandLists(_countof(lists), lists);
        Utils::throw_if_failed(swapchain_->Present(0, DXGI_PRESENT_ALLOW_TEARING), "swapchain present");
    }
    void RendererVisBuf::init_passes() {
        const auto scene = get_scene_resources();
        mesh_buffer_ = VisBufResourceBuilder::build_mesh_buffer(
            device_.Get(), command_list_.Get(), command_allocator_[frame_index_].Get(),
            command_queue_.Get(), fence_, scene);
        VisBufResources visbuf{ vis_buffer_.Get(), mesh_buffer_.Get(), scene };
        PassVisibilityResources v{}; v.visibility=vis_buffer_.Get(); v.depth=depth_stencil_buffer_.Get();
        v.constant_buffers[0]=buf_constant_[0].Get(); v.constant_buffers[1]=buf_constant_[1].Get();
        v.scene = scene;
        pass_visibility_.init(device_.Get(), *program_arguments_, v);
        PassVisBufResolveResources r{}; r.back_buffers[0]=render_targets_[0].Get(); r.back_buffers[1]=render_targets_[1].Get();
        r.visbuf=visbuf; r.constant_buffers[0]=buf_constant_[0].Get(); r.constant_buffers[1]=buf_constant_[1].Get();
        r.sampler_heap=sampler_heap_.Get(); pass_resolve_.init(device_.Get(), *program_arguments_, r);
    }
}
