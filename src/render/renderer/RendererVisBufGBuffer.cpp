#include "render/renderer/RendererVisBufGBuffer.h"

#include <d3d12.h>
#include <string>

#include "util/Utils.h"
#include "dx_util/ResourceUtils.h"
#include "engine/MaterialGPU.h"
#include "dx_util/ShaderUtils.h"

namespace rndr {

    void RendererVisBufGBuffer::make_programresult(util::ProgramResult& result) {
        result.renderer_name = "VisBuf";
        result.pass_names[0] = "visibility";
        result.pass_names[1] = "gbuffer";
        result.pass_names[2] = "lighting";
    }

    void RendererVisBufGBuffer::create_renderer_resources() {

        util::Logger::g_logger.assert_with_log(
            program_arguments_->gbuffer_cnt > 0,
            "gbuffer count must > 0 in deferred"
        );
        util::Logger::g_logger.assert_with_log(
            program_arguments_->gbuffer_cnt <= 8,
            "gbuffer count must < 8 in deferred"
        );

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
        
        for (uint32_t i = 0; i < program_arguments_->gbuffer_cnt; ++i) {

            gbuffers_.emplace_back();

            D3D12_CLEAR_VALUE gbuffer_clear_value{};
            gbuffer_clear_value.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            gbuffer_clear_value.Color[0] = CLEAR_COLOR_[0];
            gbuffer_clear_value.Color[1] = CLEAR_COLOR_[1];
            gbuffer_clear_value.Color[2] = CLEAR_COLOR_[2];
            gbuffer_clear_value.Color[3] = CLEAR_COLOR_[3];

            gbuffers_.back() = dxutl::create_texture2d(device_.Get(), width_, height_,
                DXGI_FORMAT_R32G32B32A32_FLOAT,
                D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                &gbuffer_clear_value);
        }
    }

    void RendererVisBufGBuffer::render_() {
        Utils::throw_if_failed(command_allocator_[frame_index_]->Reset(), "reset command allocator");
        Utils::throw_if_failed(command_list_->Reset(command_allocator_[frame_index_].Get(), nullptr),
            "command list reset on render start");
        copy_camera_data();

        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 0);
        pass_visibility_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 0);
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_gbuffer_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 2);
        pass_lighting_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 2);
        Utils::throw_if_failed(command_list_->Close(), "command list close on frame end");
        graphics_queue_.execute(command_list_.Get());
        present();
    }

    void RendererVisBufGBuffer::init_passes() {
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

        PassVisBufGBufferResources gbuffer{};
        gbuffer.frame_manager = &resource_manager_frame_;
        gbuffer.shader_manager = &resource_manager_shader_;
        gbuffer.visibility = vis_buffer_.Get();
        gbuffer.vertex_buffer = scene_gpu_->vertex_buffer.Get();
        gbuffer.index_buffer = scene_gpu_->index_buffer.Get();
        gbuffer.mesh_buffer = scene_gpu_->mesh_buffer.Get();
        gbuffer.instance_buffer = scene_gpu_->object_buffer.Get();
        gbuffer.material_buffer = scene_gpu_->material_buffer.Get();
        gbuffer.scene = scene_cpu_.get();
        for (const auto& texture : dummy_textures_)
            gbuffer.material_textures.push_back(texture.Get());
        gbuffer.gbuffer_count = program_arguments_->gbuffer_cnt;
        for (UINT i = 0; i < gbuffer.gbuffer_count; ++i)
            gbuffer.gbuffers[i] = gbuffers_[i].Get();
        gbuffer.constant_buffers[0] = buf_constant_[0].get();
        gbuffer.constant_buffers[1] = buf_constant_[1].get();
        gbuffer.sampler_manager = &resource_manager_sampler_;
        pass_gbuffer_.init(device_.Get(), *program_arguments_, gbuffer);

        PassDeferredLightingResources lighting{};
        lighting.frame_manager = &resource_manager_frame_;
        lighting.shader_manager = &resource_manager_shader_;
        lighting.back_buffers[0] = render_targets_[0].Get();
        lighting.back_buffers[1] = render_targets_[1].Get();
        lighting.gbuffer_count = program_arguments_->gbuffer_cnt;
        for (UINT i = 0; i < lighting.gbuffer_count; ++i)
            lighting.gbuffers[i] = gbuffers_[i].Get();
        pass_lighting_.init(device_.Get(), *program_arguments_, lighting);
    }
}
