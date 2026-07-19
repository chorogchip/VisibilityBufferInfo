#include "render/renderer/RendererVisBufGBuffer.h"

#include <d3d12.h>
#include <string>

#include "util/Utils.h"
#include "dx_util/ResourceUtils.h"
#include "engine/MaterialGPU.h"
#include "dx_util/ShaderUtils.h"
#include "dx_util/DescriptorUtils.h"
#include "render/VisBufResourceBuilder.h"

namespace rndr {

    void RendererVisBufGBuffer::make_programresult(util::ProgramResult& result) {
        result.renderer_name = "VisBuf";
        result.pass_name_0 = "visibility";
        result.pass_name_1 = "gbuffer";
        result.pass_name_2 = "lighting";
        result.pass_name_3 = "total";
    }

    void RendererVisBufGBuffer::create_pass_resources() {

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
        Utils::throw_if_failed(command_list_->Reset(command_allocator_[frame_index_].Get(), nullptr), "command list reset on render start");
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
        ID3D12CommandList* lists[] = { command_list_.Get() };
        command_queue_->ExecuteCommandLists(_countof(lists), lists);
        Utils::throw_if_failed(swapchain_->Present(0, DXGI_PRESENT_ALLOW_TEARING), "swapchain present");
    }
    void RendererVisBufGBuffer::init_passes() {
        const auto scene = get_scene_resources();
        mesh_buffer_ = VisBufResourceBuilder::build_mesh_buffer(
            device_.Get(), command_list_.Get(), command_allocator_[frame_index_].Get(),
            command_queue_.Get(), fence_, scene);
        VisBufResources visbuf{ vis_buffer_.Get(), mesh_buffer_.Get(), scene };
        PassVisibilityResources v{}; v.visibility=vis_buffer_.Get(); v.depth=depth_stencil_buffer_.Get();
        v.constant_buffers[0]=buf_constant_[0].Get(); v.constant_buffers[1]=buf_constant_[1].Get();
        v.scene = scene;
        pass_visibility_.init(device_.Get(), *program_arguments_, v);
        PassVisBufGBufferResources g{}; g.visbuf=visbuf; g.gbuffer_count=program_arguments_->gbuffer_cnt;
        for(UINT i=0;i<g.gbuffer_count;++i) g.gbuffers[i]=gbuffers_[i].Get();
        g.constant_buffers[0]=buf_constant_[0].Get(); g.constant_buffers[1]=buf_constant_[1].Get(); g.sampler_heap=sampler_heap_.Get();
        pass_gbuffer_.init(device_.Get(), *program_arguments_, g);
        PassDeferredLightingResources l{}; l.back_buffers[0]=render_targets_[0].Get(); l.back_buffers[1]=render_targets_[1].Get();
        l.gbuffer_count=program_arguments_->gbuffer_cnt; for(UINT i=0;i<l.gbuffer_count;++i) l.gbuffers[i]=gbuffers_[i].Get();
        pass_lighting_.init(device_.Get(), *program_arguments_, l);
    }
}
