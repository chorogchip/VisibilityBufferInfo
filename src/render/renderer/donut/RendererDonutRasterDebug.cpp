#include "render/renderer/donut/RendererDonutRasterDebug.h"

#include "render/renderer/donut/DonutFrameConstantsBuilder.h"

namespace rndr {

    void RendererDonutRasterDebug::init2_() {
        program_result_.renderer_name = "DonutRasterDebugReference";
        program_result_.pass_names[1] = "raster_debug_reference";

        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
            gbuffer_constants_[i].init(device_.Get());
            gbuffer_constant_resources_[i].init(
                gbuffer_constants_[i].get(),
                D3D12_RESOURCE_STATE_GENERIC_READ);
        }

        PassDonutRasterDebugResources debug_resources{};
        debug_resources.frame_manager = &resource_manager_frame_;
        debug_resources.sampler_manager = &resource_manager_sampler_;
        debug_resources.shader_manager = &resource_manager_shader_;
        debug_resources.back_buffers[0] = &render_targets_[0];
        debug_resources.back_buffers[1] = &render_targets_[1];
        debug_resources.depth = &depth_stencil_buffer_;
        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
            debug_resources.constant_buffers[i] =
                &gbuffer_constant_resources_[i];
        }
        debug_resources.scene = scene_gpu_.get();
        debug_resources.draw_stream = &draw_stream_;
        pass_debug_.init(
            device_.Get(), program_argument_, mode_, debug_resources);
    }

    void RendererDonutRasterDebug::render_prepare_donut_() {
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

    void RendererDonutRasterDebug::render_record_() {
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_debug_.render(
            command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);
    }

}
