#include "render/renderer/donut/RendererDonutVisDebug.h"

#include "dx_util/ResourceUtils.h"
#include "render/renderer/donut/DonutFrameConstantsBuilder.h"

namespace rndr {

    void RendererDonutVisDebug::init2_() {
        program_result_.renderer_name = "DonutVisDebug";
        program_result_.pass_names[1] = "visibility";
        program_result_.pass_names[2] = "visibility_debug";

        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
            gbuffer_constants_[i].init(device_.Get());
            gbuffer_constant_resources_[i].init(
                gbuffer_constants_[i].get(),
                D3D12_RESOURCE_STATE_GENERIC_READ);
        }

        D3D12_CLEAR_VALUE clear{};
        clear.Format = DXGI_FORMAT_R32G32_UINT;
        visibility_buffer_.init(dxutl::create_texture2d(
            device_.Get(), width_, height_, DXGI_FORMAT_R32G32_UINT,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            &clear).Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        PassDonutVisibilityResources visibility{};
        visibility.frame_manager = &resource_manager_frame_;
        visibility.sampler_manager = &resource_manager_sampler_;
        visibility.shader_manager = &resource_manager_shader_;
        visibility.depth = &depth_stencil_buffer_;
        visibility.visibility_buf = &visibility_buffer_;
        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i)
            visibility.constant_buffers[i] = &gbuffer_constant_resources_[i];
        visibility.scene = scene_gpu_.get();
        visibility.draw_stream = &draw_stream_;
        pass_visibility_.init(device_.Get(), program_argument_, visibility, false);

        PassDonutVisDebugResolveResources debug_resolve{};
        debug_resolve.frame_manager = &resource_manager_frame_;
        debug_resolve.shader_manager = &resource_manager_shader_;
        debug_resolve.back_buffers[0] = &render_targets_[0];
        debug_resolve.back_buffers[1] = &render_targets_[1];
        debug_resolve.visibility = &visibility_buffer_;
        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i)
            debug_resolve.constant_buffers[i] = &gbuffer_constant_resources_[i];
        debug_resolve.scene = scene_gpu_.get();
        pass_debug_resolve_.init(
            device_.Get(), program_argument_, mode_, debug_resolve);
    }

    void RendererDonutVisDebug::render_prepare_donut_() {
        const scene::DonutPlanarViewConstants current_view =
            DonutFrameConstantsBuilder::make_planar_view(
                camera_, width_, height_);
        const scene::DonutPlanarViewConstants previous_view =
            has_previous_view_constants_ ?
            previous_view_constants_ : current_view;

        gbuffer_constants_[frame_index_].buffer =
            DonutFrameConstantsBuilder::make_gbuffer_constants(
                current_view, previous_view);
        gbuffer_constants_[frame_index_].update();

        previous_view_constants_ = current_view;
        has_previous_view_constants_ = true;
    }

    void RendererDonutVisDebug::render_record_() {
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
