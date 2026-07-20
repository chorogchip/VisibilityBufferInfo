#include "render/renderer/benchmark/RendererForward.h"

#include <d3d12.h>
#include <string>

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"

namespace rndr {

    RendererForward::RendererForward(bool do_prepass)
        : do_prepass_(do_prepass) {}

    void RendererForward::init_programresult_(util::ProgramResult& result) {
        result.renderer_name = do_prepass_ ? "ForwardPrepass" : "Forward";
        if (do_prepass_)
            result.pass_names[0] = "depth_prepass";
        result.pass_names[1] = "forward";
    }

    void RendererForward::record_render_commands_() {
        if (do_prepass_) {
            frame_time_.start_timestamp(command_list_.Get(), frame_index_, 0);
            pass_depth_pre_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
            frame_time_.end_timestamp(command_list_.Get(), frame_index_, 0);
        }

        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_forward_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);
    }

    void RendererForward::init_passes_()
    {
        if (do_prepass_) {
            PassDepthPreResources depth_resources{};
            depth_resources.frame_manager = &resource_manager_frame_;
            depth_resources.shader_manager = &resource_manager_shader_;
            depth_resources.depth = &depth_stencil_buffer_;
            depth_resources.constant_buffers[0] = buf_constant_[0].get();
            depth_resources.constant_buffers[1] = buf_constant_[1].get();
            depth_resources.instance_buffer = scene_gpu_->object_buffer.Get();
            depth_resources.vertex_buffer_view = scene_gpu_->vertex_buffer_view;
            depth_resources.index_buffer_view = scene_gpu_->index_buffer_view;
            depth_resources.scene = scene_cpu_.get();
            pass_depth_pre_.init(device_.Get(), benchmark_program_arguments(), depth_resources);
        }

        PassForwardResources resources{};
        resources.frame_manager = &resource_manager_frame_;
        resources.shader_manager = &resource_manager_shader_;
        resources.back_buffers[0] = &render_targets_[0];
        resources.back_buffers[1] = &render_targets_[1];
        resources.depth = &depth_stencil_buffer_;
        resources.constant_buffers[0] = buf_constant_[0].get();
        resources.constant_buffers[1] = buf_constant_[1].get();
        resources.instance_buffer = scene_gpu_->object_buffer.Get();
        resources.material_buffer = scene_gpu_->material_buffer.Get();
        for (const auto& texture : material_textures())
            resources.material_textures.push_back(texture.Get());
        resources.sampler_manager = &resource_manager_sampler_;
        resources.vertex_buffer_view = scene_gpu_->vertex_buffer_view;
        resources.index_buffer_view = scene_gpu_->index_buffer_view;
        resources.scene = scene_cpu_.get();

        pass_forward_.init(device_.Get(), benchmark_program_arguments(), resources, do_prepass_);
    }
}
