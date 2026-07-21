#include "render/renderer/benchmark/RendererDeferred.h"

#include "dx_util/ResourceUtils.h"
#include "util/Logger.h"

namespace rndr {

    RendererDeferred::RendererDeferred(bool do_prepass)
        : do_prepass_(do_prepass) {}

    void RendererDeferred::init2_() {

        program_result_.renderer_name = do_prepass_ ? "DeferredPrepass" : "Deferred";
        if (do_prepass_)
            program_result_.pass_names[0] = "depth_prepass";
        program_result_.pass_names[1] = "geometry";
        program_result_.pass_names[2] = "lighting";

        util::Logger::g_logger.assert_with_log(
            program_argument_.gbuffer_cnt > 0 && program_argument_.gbuffer_cnt <= 8,
            "gbuffer count must be between 1 and 8 in deferred");
        for (UINT i = 0; i < program_argument_.gbuffer_cnt; ++i) {
            D3D12_CLEAR_VALUE clear{};
            clear.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            clear.Color[0] = .1f;
            clear.Color[1] = .1f;
            clear.Color[2] = .15f;
            clear.Color[3] = 1.f;
            auto gbuffer = dxutl::create_texture2d(device_.Get(), width_, height_,
                DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, &clear);
            gbuffers_.emplace_back();
            gbuffers_.back().init(gbuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        PassGBufferResources gbuffer{};
        gbuffer.frame_manager = &resource_manager_frame_;
        gbuffer.shader_manager = &resource_manager_shader_;
        gbuffer.gbuffer_count = program_argument_.gbuffer_cnt;
        for (UINT i = 0; i < gbuffer.gbuffer_count; ++i)
            gbuffer.gbuffers[i] = &gbuffers_[i];
        gbuffer.depth = &depth_stencil_buffer_;
        gbuffer.constant_buffers[0] = buf_constant_[0].get();
        gbuffer.constant_buffers[1] = buf_constant_[1].get();
        gbuffer.instance_buffer = scene_gpu_->object_buffer.Get();
        gbuffer.material_buffer = scene_gpu_->material_buffer.Get();
        for (const auto& texture : textures_)
            gbuffer.material_textures.push_back(texture.Get());
        gbuffer.sampler_manager = &resource_manager_sampler_;
        gbuffer.vertex_buffer_view = scene_gpu_->vertex_buffer_view;
        gbuffer.index_buffer_view = scene_gpu_->index_buffer_view;
        gbuffer.scene = scene_cpu_.get();

        if (do_prepass_) {
            PassDepthPreResources depth{};
            depth.frame_manager = &resource_manager_frame_;
            depth.shader_manager = &resource_manager_shader_;
            depth.depth = &depth_stencil_buffer_;
            depth.constant_buffers[0] = buf_constant_[0].get();
            depth.constant_buffers[1] = buf_constant_[1].get();
            depth.instance_buffer = scene_gpu_->object_buffer.Get();
            depth.vertex_buffer_view = scene_gpu_->vertex_buffer_view;
            depth.index_buffer_view = scene_gpu_->index_buffer_view;
            depth.scene = scene_cpu_.get();
            pass_depth_pre_.init(device_.Get(), program_argument_, depth);
        }
        pass_gbuffer_.init(device_.Get(), program_argument_, gbuffer, do_prepass_);

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

    void RendererDeferred::render_record_() {
        if (do_prepass_) {
            frame_time_.start_timestamp(command_list_.Get(), frame_index_, 0);
            pass_depth_pre_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
            frame_time_.end_timestamp(command_list_.Get(), frame_index_, 0);
        }
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_gbuffer_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 2);
        pass_lighting_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 2);

    }
}
