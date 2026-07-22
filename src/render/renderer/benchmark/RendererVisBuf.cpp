#include "render/renderer/benchmark/RendererVisBuf.h"

#include <d3d12.h>
#include <string>

#include "dx_util/ResourceUtils.h"
#include "engine/MaterialGPU.h"
#include "dx_util/ShaderUtils.h"

namespace rndr {

    void RendererVisBuf::init2_() {
        program_result_.renderer_name = "VisBuf";
        program_result_.pass_names[1] = "visibility";
        program_result_.pass_names[2] = "resolve";

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
        vis_buffer_.init(vis_buffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        PassVisibilityResources visibility{};
        visibility.frame_manager = &resource_manager_frame_;
        visibility.visibility = &vis_buffer_;
        visibility.depth = &depth_stencil_buffer_;
        visibility.constant_buffer_addresses[0] =
            buf_constant_[0].get()->GetGPUVirtualAddress();
        visibility.constant_buffer_addresses[1] =
            buf_constant_[1].get()->GetGPUVirtualAddress();
        static_assert(util::Constants::FRAME_COUNT == 2);
        visibility.instance_buffer_address =
            scene_object_buffer_.get()->GetGPUVirtualAddress();
        visibility.vertex_buffer_view = scene_gpu_->vertex_buffer_view;
        visibility.index_buffer_view = scene_gpu_->index_buffer_view;
        visibility.scene = scene_cpu_.get();
        pass_visibility_.init(device_.Get(), program_argument_, visibility);

        PassVisBufResolveResources resolve{};
        resolve.frame_manager = &resource_manager_frame_;
        resolve.shader_manager = &resource_manager_shader_;
        resolve.back_buffers[0] = &render_targets_[0];
        resolve.back_buffers[1] = &render_targets_[1];
        static_assert(util::Constants::FRAME_COUNT == 2);
        resolve.visibility = &vis_buffer_;
        resolve.vertex_buffer = &scene_vertex_buffer_;
        resolve.index_buffer = &scene_index_buffer_;
        resolve.mesh_buffer = &scene_mesh_buffer_;
        resolve.instance_buffer = &scene_object_buffer_;
        resolve.material_buffer = &scene_material_buffer_;
        for (auto& texture : textures_)
            resolve.material_textures.push_back(&texture);
        resolve.scene = scene_cpu_.get();
        resolve.constant_buffer_addresses[0] =
            buf_constant_[0].get()->GetGPUVirtualAddress();
        resolve.constant_buffer_addresses[1] =
            buf_constant_[1].get()->GetGPUVirtualAddress();
        static_assert(util::Constants::FRAME_COUNT == 2);
        resolve.sampler_manager = &resource_manager_sampler_;
        pass_resolve_.init(device_.Get(), program_argument_, resolve);
    }

    void RendererVisBuf::render_record_() {
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_visibility_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 2);
        pass_resolve_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 2);
    }
}
