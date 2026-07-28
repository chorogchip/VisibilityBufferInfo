#include "render/renderer/benchmark/RendererVisBufDebug.h"

#include "dx_util/ResourceUtils.h"

namespace rndr {

    void RendererVisBufDebug::init2_() {
        program_result_.renderer_name = "VisBufDebug";
        program_result_.pass_names[1] = "visibility";
        program_result_.pass_names[2] = "visibility_debug";

        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = DXGI_FORMAT_R32G32_UINT;

        visibility_buffer_.init(dxutl::create_texture2d(
            device_.Get(), width_, height_, DXGI_FORMAT_R32G32_UINT,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            &clear_value).Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        PassVisibilityResources visibility{};
        visibility.frame_manager = &resource_manager_frame_;
        visibility.visibility = &visibility_buffer_;
        visibility.depth = &depth_stencil_buffer_;
        visibility.constant_buffer_addresses[0] =
            buf_constant_[0].get()->GetGPUVirtualAddress();
        visibility.constant_buffer_addresses[1] =
            buf_constant_[1].get()->GetGPUVirtualAddress();
        static_assert(util::Constants::FRAME_COUNT == 2);
        visibility.instance_buffer_address =
            scene_instance_buffer_.get()->GetGPUVirtualAddress();
        visibility.draw_instance_buffer_address =
            scene_draw_instance_buffer_.get()->GetGPUVirtualAddress();
        visibility.draw_instance_id_buffer_address =
            scene_draw_instance_id_buffer_.get()->GetGPUVirtualAddress();
        visibility.vertex_buffer_view = scene_gpu_->vertex_buffer_view;
        visibility.index_buffer_view = scene_gpu_->index_buffer_view;
        visibility.scene = scene_cpu_.get();
        visibility.draw_stream = &draw_stream_;
        pass_visibility_.init(device_.Get(), program_argument_, visibility);

        PassVisBufDebugResolveResources debug_resolve{};
        debug_resolve.frame_manager = &resource_manager_frame_;
        debug_resolve.shader_manager = &resource_manager_shader_;
        debug_resolve.back_buffers[0] = &render_targets_[0];
        debug_resolve.back_buffers[1] = &render_targets_[1];
        static_assert(util::Constants::FRAME_COUNT == 2);
        debug_resolve.visibility = &visibility_buffer_;
        debug_resolve.vertex_buffer = &scene_vertex_buffer_;
        debug_resolve.index_buffer = &scene_index_buffer_;
        debug_resolve.submesh_buffer = &scene_submesh_buffer_;
        debug_resolve.instance_buffer = &scene_instance_buffer_;
        debug_resolve.draw_instance_buffer = &scene_draw_instance_buffer_;
        debug_resolve.scene = scene_cpu_.get();
        debug_resolve.constant_buffer_addresses[0] =
            buf_constant_[0].get()->GetGPUVirtualAddress();
        debug_resolve.constant_buffer_addresses[1] =
            buf_constant_[1].get()->GetGPUVirtualAddress();
        pass_debug_resolve_.init(
            device_.Get(), program_argument_, mode_, debug_resolve);
    }

    void RendererVisBufDebug::render_record_() {
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_visibility_.render(
            command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);

        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 2);
        pass_debug_resolve_.render(
            command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 2);
    }

}
