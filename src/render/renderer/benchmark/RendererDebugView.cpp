#include "render/renderer/benchmark/RendererDebugView.h"

#include <d3d12.h>
#include <string>

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"

namespace rndr {

    RendererDebugView::RendererDebugView() {}

    void RendererDebugView::init2_() {
        program_result_.renderer_name = "Debug";

        PassDebugViewResources resources{};
        resources.frame_manager = &resource_manager_frame_;
        resources.back_buffers[0] = &render_targets_[0];
        resources.back_buffers[1] = &render_targets_[1];
        static_assert(util::Constants::FRAME_COUNT == 2);
        resources.depth = &depth_stencil_buffer_;
        resources.constant_buffer_addresses[0] =
            buf_constant_[0].get()->GetGPUVirtualAddress();
        resources.constant_buffer_addresses[1] =
            buf_constant_[1].get()->GetGPUVirtualAddress();
        static_assert(util::Constants::FRAME_COUNT == 2);
        resources.instance_buffer_address =
            scene_object_buffer_.get()->GetGPUVirtualAddress();
        resources.material_buffer_address =
            scene_material_buffer_.get()->GetGPUVirtualAddress();
        for (auto& texture : textures_)
            resources.material_textures.push_back(&texture);
        resources.vertex_buffer_view = scene_gpu_->vertex_buffer_view;
        resources.index_buffer_view = scene_gpu_->index_buffer_view;
        resources.scene = scene_cpu_.get();
        resources.materials = &scene_gpu_->materials;
        pass_debug_view_.init(device_.Get(), program_argument_, resources);
    }

    void RendererDebugView::render_record_() {
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_debug_view_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);
    }
}
