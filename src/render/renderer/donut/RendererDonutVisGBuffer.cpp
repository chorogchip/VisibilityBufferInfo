#include "render/renderer/donut/RendererDonutVisGBuffer.h"

#include <cstdint>
#include <vector>

#include "dx_util/ResourceUtils.h"
#include "render/renderer/donut/DonutFrameConstantsBuilder.h"
#include "util/Constants.h"
#include "util/RenderConstants.h"
#include "util/Utils.h"

namespace rndr {

    void RendererDonutVisGBuffer::init2_() {
        program_result_.renderer_name = "DonutVisGBuffer";
        program_result_.pass_names[1] = "visibility";
        program_result_.pass_names[2] = "gbuffer";
        program_result_.pass_names[3] = "lighting";
        program_result_.pass_names[4] = "tonemap";

        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
            gbuffer_constants_[i].init(device_.Get());
            lighting_constants_[i].init(device_.Get());

            gbuffer_constant_resources_[i].init(
                gbuffer_constants_[i].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
            lighting_constant_resources_[i].init(
                lighting_constants_[i].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
        }

        {
            D3D12_CLEAR_VALUE clear{};
            clear.Format = DXGI_FORMAT_R32G32_UINT;
            clear.Color[0] = 0.0f;
            clear.Color[1] = 0.0f;
            clear.Color[2] = 0.0f;
            clear.Color[3] = 0.0f;

            visibility_buffer_.init(dxutl::create_texture2d(
                device_.Get(),
                width_,
                height_,
                DXGI_FORMAT_R32G32_UINT,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                &clear).Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        for (UINT i = 0; i < GBUFFER_COUNT; ++i) {
            D3D12_CLEAR_VALUE clear{};
            clear.Format = util::RenderConstants::DONUT_GBUFFER_FORMATS[i];
            clear.Color[0] = 0.0f;
            clear.Color[1] = 0.0f;
            clear.Color[2] = 0.0f;
            clear.Color[3] = 0.0f;

            gbuffers_[i].init(dxutl::create_texture2d(
                device_.Get(),
                width_,
                height_,
                util::RenderConstants::DONUT_GBUFFER_FORMATS[i],
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                &clear).Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        {
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> used_upload_heaps;
            util::Utils::throw_if_failed(
                command_allocator_[frame_index_]->Reset(),
                "reset command allocator on Donut visibility neutral resource creation");
            util::Utils::throw_if_failed(
                command_list_->Reset(command_allocator_[frame_index_].Get(), nullptr),
                "reset command list on Donut visibility neutral resource creation");
            neutral_resources_.init(device_.Get(), command_list_.Get(), used_upload_heaps);
            util::Utils::throw_if_failed(
                command_list_->Close(),
                "close command list on Donut visibility neutral resource creation");
            graphics_queue_.execute(command_list_.Get());
            graphics_queue_.wait_idle();
        }

        const std::uint32_t zero_exposure = 0;
        auto exposure = dxutl::create_buffer(
            device_.Get(),
            sizeof(zero_exposure),
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_RESOURCE_STATE_GENERIC_READ);
        dxutl::copy_to_upload_buffer(
            exposure.Get(), &zero_exposure, sizeof(zero_exposure));
        exposure_buffer_.init(exposure.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);

        hdr_color_buffer_.init(
            dxutl::create_texture2d(
                device_.Get(),
                width_,
                height_,
                DXGI_FORMAT_R16G16B16A16_FLOAT,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS).Get(),
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
        pass_visibility_.init(device_.Get(), program_argument_, visibility, false);

        PassDonutVisGBufferResources gbuffer{};
        gbuffer.frame_manager = &resource_manager_frame_;
        gbuffer.sampler_manager = &resource_manager_sampler_;
        gbuffer.shader_manager = &resource_manager_shader_;
        gbuffer.visibility = &visibility_buffer_;
        gbuffer.depth = &depth_stencil_buffer_;
        for (UINT i = 0; i < GBUFFER_COUNT; ++i)
            gbuffer.gbuffers[i] = &gbuffers_[i];
        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i)
            gbuffer.constant_buffers[i] = &gbuffer_constant_resources_[i];
        gbuffer.scene = scene_gpu_.get();
        pass_gbuffer_.init(device_.Get(), program_argument_, gbuffer);

        PassDonutDeferredLightingResources lighting{};
        lighting.shader_manager = &resource_manager_shader_;
        lighting.sampler_manager = &resource_manager_sampler_;
        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i)
            lighting.constant_buffers[i] = &lighting_constant_resources_[i];
        lighting.buf_shadow_map =
            &neutral_resources_.black_depth_stencil_texture_2d_array;
        lighting.buf_diffuse_light_probe =
            &neutral_resources_.black_cube_map_array;
        lighting.buf_specular_light_probe =
            &neutral_resources_.black_cube_map_array;
        lighting.buf_env_brdf = &neutral_resources_.black_texture;
        lighting.depth = &depth_stencil_buffer_;
        for (UINT i = 0; i < GBUFFER_COUNT; ++i)
            lighting.gbuffers[i] = &gbuffers_[i];
        lighting.buf_ibl_diffuse = &neutral_resources_.black_texture;
        lighting.buf_ibl_specular = &neutral_resources_.black_texture;
        lighting.buf_shadow = &neutral_resources_.black_texture;
        lighting.buf_ao = &neutral_resources_.white_texture;
        lighting.uav_output = &hdr_color_buffer_;
        pass_lighting_.init(device_.Get(), program_argument_, lighting);

        PassDonutTonemapResources tonemap{};
        tonemap.frame_manager = &resource_manager_frame_;
        tonemap.shader_manager = &resource_manager_shader_;
        tonemap.sampler_manager = &resource_manager_sampler_;
        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i)
            tonemap.back_buffers[i] = &render_targets_[i];
        tonemap.hdr_color_buffer = &hdr_color_buffer_;
        tonemap.exposure_buffer = &exposure_buffer_;
        pass_tonemap_.init(device_.Get(), program_argument_, tonemap);
    }

    void RendererDonutVisGBuffer::render_prepare_donut_() {
        const scene::DonutPlanarViewConstants current_view =
            DonutFrameConstantsBuilder::make_planar_view(
                camera_, width_, height_);
        const scene::DonutPlanarViewConstants previous_view =
            has_previous_view_constants_ ?
            previous_view_constants_ :
            current_view;

        gbuffer_constants_[frame_index_].buffer =
            DonutFrameConstantsBuilder::make_gbuffer_constants(
                current_view, previous_view);
        gbuffer_constants_[frame_index_].update();

        lighting_constants_[frame_index_].buffer =
            DonutFrameConstantsBuilder::make_deferred_lighting_constants(
                current_view, donut_frame_index_);
        lighting_constants_[frame_index_].update();

        previous_view_constants_ = current_view;
        has_previous_view_constants_ = true;
        ++donut_frame_index_;
    }

    void RendererDonutVisGBuffer::render_record_() {
        record_render_instance_upload_(command_list_.Get());

        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_visibility_.render(
            command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);

        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 2);
        pass_gbuffer_.render(
            command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 2);

        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 3);
        pass_lighting_.render(command_list_.Get(), frame_index_, width_, height_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 3);

        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 4);
        pass_tonemap_.render(
            command_list_.Get(), frame_index_, viewport_, scissor_rect_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 4);
    }
}
