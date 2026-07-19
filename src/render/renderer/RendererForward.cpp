#include "render/renderer/RendererForward.h"

#include <d3d12.h>
#include <string>

#include "util/Utils.h"
#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"

namespace rndr {

    RendererForward::RendererForward(bool do_prepass)
        : do_prepass_(do_prepass) {}

    void RendererForward::make_programresult(util::ProgramResult& result) {
        result.renderer_name = do_prepass_ ? "ForwardPrepass" : "Forward";
        if (do_prepass_)
            result.pass_names[0] = "depth_prepass";
        result.pass_names[1] = "forward";
    }

    void RendererForward::render_() {

        Utils::throw_if_failed(command_allocator_[frame_index_]->Reset(), "reset command allocator");
        Utils::throw_if_failed(command_list_->Reset(command_allocator_[frame_index_].Get(),
            nullptr), "command list reset on render start");
        this->copy_camera_data();

        if (do_prepass_) {
            frame_time_.start_timestamp(command_list_.Get(), frame_index_, 0);
            pass_depth_pre_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
            frame_time_.end_timestamp(command_list_.Get(), frame_index_, 0);
        }

        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_forward_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);
        Utils::throw_if_failed(command_list_->Close(), "command list clonse on framne end");

        graphics_queue_.execute(command_list_.Get());
        Utils::throw_if_failed(swapchain_->Present(0, DXGI_PRESENT_ALLOW_TEARING), "swapchain present");
    }

    D3D12_RESOURCE_STATES RendererForward::depth_stencil_initial_state() const {
        return do_prepass_
            ? D3D12_RESOURCE_STATE_DEPTH_READ
            : D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    
    void RendererForward::init_passes()
    {
        if (do_prepass_) {
            PassDepthPreResources depth_resources{};
            depth_resources.frame_manager = &resource_manager_frame_;
            depth_resources.shader_manager = &resource_manager_shader_;
            depth_resources.depth = depth_stencil_buffer_.Get();
            depth_resources.constant_buffers[0] = buf_constant_[0].Get();
            depth_resources.constant_buffers[1] = buf_constant_[1].Get();
            depth_resources.instance_buffer = scene_gpu_->object_buffer.Get();
            depth_resources.vertex_buffer_view = scene_gpu_->vertex_buffer_view;
            depth_resources.index_buffer_view = scene_gpu_->index_buffer_view;
            depth_resources.scene = scene_cpu_.get();
            pass_depth_pre_.init(device_.Get(), *program_arguments_, depth_resources);
        }

        PassForwardResources resources{};
        resources.frame_manager = &resource_manager_frame_;
        resources.shader_manager = &resource_manager_shader_;
        resources.back_buffers[0] = render_targets_[0].Get();
        resources.back_buffers[1] = render_targets_[1].Get();
        resources.depth = depth_stencil_buffer_.Get();
        resources.constant_buffers[0] = buf_constant_[0].Get();
        resources.constant_buffers[1] = buf_constant_[1].Get();
        resources.instance_buffer = scene_gpu_->object_buffer.Get();
        resources.material_buffer = scene_gpu_->material_buffer.Get();
        for (const auto& texture : dummy_textures_)
            resources.material_textures.push_back(texture.Get());
        resources.sampler_heap = sampler_heap_.Get();
        resources.vertex_buffer_view = scene_gpu_->vertex_buffer_view;
        resources.index_buffer_view = scene_gpu_->index_buffer_view;
        resources.scene = scene_cpu_.get();

        pass_forward_.init(device_.Get(), *program_arguments_, resources, do_prepass_);
    }
}
