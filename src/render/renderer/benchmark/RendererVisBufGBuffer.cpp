#include "render/renderer/benchmark/RendererVisBufGBuffer.h"

#include <d3d12.h>
#include <string>

#include "util/RenderConstants.h"
#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "engine/MaterialGPU.h"

namespace rndr {

    void RendererVisBufGBuffer::init2_() {
        program_result_.renderer_name = "VisBuf";
        program_result_.pass_names[1] = "visibility";
        program_result_.pass_names[2] = "gbuffer";
        program_result_.pass_names[3] = "lighting";


        util::Logger::g_logger.assert_with_log(
            program_argument_.gbuffer_cnt > 0,
            "gbuffer count must > 0 in deferred"
        );
        util::Logger::g_logger.assert_with_log(
            program_argument_.gbuffer_cnt <= 8,
            "gbuffer count must < 8 in deferred"
        );

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

        for (uint32_t i = 0; i < program_argument_.gbuffer_cnt; ++i) {
            D3D12_CLEAR_VALUE gbuffer_clear_value{};
            gbuffer_clear_value.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            gbuffer_clear_value.Color[0] = util::RenderConstants::CLEAR_COLOR[0];
            gbuffer_clear_value.Color[1] = util::RenderConstants::CLEAR_COLOR[1];
            gbuffer_clear_value.Color[2] = util::RenderConstants::CLEAR_COLOR[2];
            gbuffer_clear_value.Color[3] = util::RenderConstants::CLEAR_COLOR[3];

            auto gbuffer = dxutl::create_texture2d(device_.Get(), width_, height_,
                DXGI_FORMAT_R32G32B32A32_FLOAT,
                D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                &gbuffer_clear_value);
            gbuffers_.emplace_back();
            gbuffers_.back().init(gbuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        PassVisibilityResources visibility{};
        visibility.frame_manager = &resource_manager_frame_;
        visibility.visibility = &vis_buffer_;
        visibility.depth = &depth_stencil_buffer_;
        visibility.constant_buffer_addresses[0] =
            buf_constant_[0].get()->GetGPUVirtualAddress();
        visibility.constant_buffer_addresses[1] =
            buf_constant_[1].get()->GetGPUVirtualAddress();
        visibility.instance_buffer_address =
            scene_object_buffer_.get()->GetGPUVirtualAddress();
        visibility.vertex_buffer_view = scene_gpu_->vertex_buffer_view;
        visibility.index_buffer_view = scene_gpu_->index_buffer_view;
        visibility.scene = scene_cpu_.get();
        pass_visibility_.init(device_.Get(), program_argument_, visibility);

        PassVisBufGBufferResources gbuffer{};
        gbuffer.frame_manager = &resource_manager_frame_;
        gbuffer.shader_manager = &resource_manager_shader_;
        gbuffer.visibility = &vis_buffer_;
        gbuffer.vertex_buffer = &scene_vertex_buffer_;
        gbuffer.index_buffer = &scene_index_buffer_;
        gbuffer.mesh_buffer = &scene_mesh_buffer_;
        gbuffer.instance_buffer = &scene_object_buffer_;
        gbuffer.material_buffer = &scene_material_buffer_;
        gbuffer.scene = scene_cpu_.get();
        for (auto& texture : textures_)
            gbuffer.material_textures.push_back(&texture);
        gbuffer.gbuffer_count = program_argument_.gbuffer_cnt;
        for (UINT i = 0; i < gbuffer.gbuffer_count; ++i)
            gbuffer.gbuffers[i] = &gbuffers_[i];
        gbuffer.constant_buffer_addresses[0] =
            buf_constant_[0].get()->GetGPUVirtualAddress();
        gbuffer.constant_buffer_addresses[1] =
            buf_constant_[1].get()->GetGPUVirtualAddress();
        gbuffer.sampler_manager = &resource_manager_sampler_;
        pass_gbuffer_.init(device_.Get(), program_argument_, gbuffer);

        PassDeferredLightingResources lighting{};
        lighting.frame_manager = &resource_manager_frame_;
        lighting.shader_manager = &resource_manager_shader_;
        lighting.back_buffers[0] = &render_targets_[0];
        lighting.back_buffers[1] = &render_targets_[1];
        lighting.gbuffer_count = program_argument_.gbuffer_cnt;
        for (UINT i = 0; i < lighting.gbuffer_count; ++i)
            lighting.gbuffers[i] = &gbuffers_[i];
        pass_lighting_.init(device_.Get(), program_argument_, lighting);
    }

    void RendererVisBufGBuffer::render_record_() {
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_visibility_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 2);
        pass_gbuffer_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 2);
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 3);
        pass_lighting_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 3);
    }
}
