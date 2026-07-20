#include "render/renderer/benchmark/RendererVisBuf.h"

#include <d3d12.h>
#include <string>

#include "dx_util/ResourceUtils.h"
#include "engine/MaterialGPU.h"
#include "dx_util/ShaderUtils.h"

namespace rndr {

    void RendererVisBuf::init_programresult_(util::ProgramResult& result) {
        result.renderer_name = "VisBuf";
        result.pass_names[0] = "visibility";
        result.pass_names[1] = "resolve";
    }

    void RendererVisBuf::init_renderer_resources_() {
        RendererBenchmark::init_renderer_resources_();

        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = DXGI_FORMAT_R32G32_UINT;
        clear_value.Color[0] = 0.0f;
        clear_value.Color[1] = 0.0f;
        clear_value.Color[2] = 0.0f;
        clear_value.Color[3] = 0.0f;

        auto vis_buffer = dxutl::create_texture2d(device_.Get(), width_, height_,
            DXGI_FORMAT_R32G32_UINT,
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            &clear_value);
        vis_buffer_.attach(vis_buffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    void RendererVisBuf::record_render_commands_() {
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 0);
        pass_visibility_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 0);
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_resolve_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);
    }

    void RendererVisBuf::init_passes_() {
        PassVisibilityResources visibility{};
        visibility.frame_manager = &resource_manager_frame_;
        visibility.shader_manager = &resource_manager_shader_;
        visibility.visibility = &vis_buffer_;
        visibility.depth = &depth_stencil_buffer_;
        visibility.constant_buffers[0] = buf_constant_[0].get();
        visibility.constant_buffers[1] = buf_constant_[1].get();
        visibility.instance_buffer = scene_gpu_->object_buffer.Get();
        visibility.vertex_buffer_view = scene_gpu_->vertex_buffer_view;
        visibility.index_buffer_view = scene_gpu_->index_buffer_view;
        visibility.scene = scene_cpu_.get();
        pass_visibility_.init(device_.Get(), benchmark_program_arguments(), visibility);

        PassVisBufResolveResources resolve{};
        resolve.frame_manager = &resource_manager_frame_;
        resolve.shader_manager = &resource_manager_shader_;
        resolve.back_buffers[0] = &render_targets_[0];
        resolve.back_buffers[1] = &render_targets_[1];
        resolve.visibility = &vis_buffer_;
        resolve.vertex_buffer = scene_gpu_->vertex_buffer.Get();
        resolve.index_buffer = scene_gpu_->index_buffer.Get();
        resolve.mesh_buffer = scene_gpu_->mesh_buffer.Get();
        resolve.instance_buffer = scene_gpu_->object_buffer.Get();
        resolve.material_buffer = scene_gpu_->material_buffer.Get();
        for (const auto& texture : material_textures())
            resolve.material_textures.push_back(texture.Get());
        resolve.scene = scene_cpu_.get();
        resolve.constant_buffers[0] = buf_constant_[0].get();
        resolve.constant_buffers[1] = buf_constant_[1].get();
        resolve.sampler_manager = &resource_manager_sampler_;
        pass_resolve_.init(device_.Get(), benchmark_program_arguments(), resolve);
    }
}
