#include "render/renderer/RendererVisBuf.h"

#include <d3d12.h>
#include <string>

#include "util/Utils.h"
#include "dx_util/ResourceUtils.h"
#include "engine/MaterialGPU.h"
#include "dx_util/ShaderUtils.h"

namespace rndr {

    void RendererVisBuf::make_programresult(util::ProgramResult& result) {
        result.renderer_name = "VisBuf";
        result.pass_names[0] = "visibility";
        result.pass_names[1] = "resolve";
    }

    void RendererVisBuf::create_renderer_resources() {
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
        Utils::throw_if_failed(command_list_->Reset(command_allocator_[frame_index_].Get(), nullptr),
            "command list reset on render start");
        copy_camera_data();

        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 0);
        pass_visibility_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 0);
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_resolve_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);
        Utils::throw_if_failed(command_list_->Close(), "command list close on frame end");
        graphics_queue_.execute(command_list_.Get());
        present();
    }

    void RendererVisBuf::init_passes() {
        PassVisibilityResources visibility{};
        visibility.frame_manager = &resource_manager_frame_;
        visibility.shader_manager = &resource_manager_shader_;
        visibility.visibility = vis_buffer_.Get();
        visibility.depth = depth_stencil_buffer_.Get();
        visibility.constant_buffers[0] = buf_constant_[0].get();
        visibility.constant_buffers[1] = buf_constant_[1].get();
        visibility.instance_buffer = scene_gpu_->object_buffer.Get();
        visibility.vertex_buffer_view = scene_gpu_->vertex_buffer_view;
        visibility.index_buffer_view = scene_gpu_->index_buffer_view;
        visibility.scene = scene_cpu_.get();
        pass_visibility_.init(device_.Get(), *program_arguments_, visibility);

        PassVisBufResolveResources resolve{};
        resolve.frame_manager = &resource_manager_frame_;
        resolve.shader_manager = &resource_manager_shader_;
        resolve.back_buffers[0] = render_targets_[0].Get();
        resolve.back_buffers[1] = render_targets_[1].Get();
        resolve.visibility = vis_buffer_.Get();
        resolve.vertex_buffer = scene_gpu_->vertex_buffer.Get();
        resolve.index_buffer = scene_gpu_->index_buffer.Get();
        resolve.mesh_buffer = scene_gpu_->mesh_buffer.Get();
        resolve.instance_buffer = scene_gpu_->object_buffer.Get();
        resolve.material_buffer = scene_gpu_->material_buffer.Get();
        for (const auto& texture : dummy_textures_)
            resolve.material_textures.push_back(texture.Get());
        resolve.scene = scene_cpu_.get();
        resolve.constant_buffers[0] = buf_constant_[0].get();
        resolve.constant_buffers[1] = buf_constant_[1].get();
        resolve.sampler_manager = &resource_manager_sampler_;
        pass_resolve_.init(device_.Get(), *program_arguments_, resolve);
    }
}
