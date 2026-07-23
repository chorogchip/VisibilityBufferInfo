#include "render/renderer/donut/RendererDonutDeferred.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "util/Constants.h"
#include "util/RenderConstants.h"
#include "util/Utils.h"
#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"
#include "engine/ResourceViewBuilder.h"
#include "engine/RootSignatureBuilder.h"
#include "render/renderer/donut/DonutFrameConstantsBuilder.h"

namespace rndr {

	RendererDonutDeferred::RendererDonutDeferred(bool do_prepass)
		: do_prepass_(do_prepass) {}

	void RendererDonutDeferred::init2_() {
		program_result_.renderer_name =
			do_prepass_ ? "DonutDeferredPrepass" : "DonutDeferred";

		if (do_prepass_)
			program_result_.pass_names[1] = "depth_prepass";
		program_result_.pass_names[do_prepass_ ? 2 : 1] = "geometry";
		program_result_.pass_names[do_prepass_ ? 3 : 2] = "lighting";
		program_result_.pass_names[do_prepass_ ? 4 : 3] = "tonemap";

		for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
			depth_constants_[i].init(device_.Get());
			gbuffer_constants_[i].init(device_.Get());
			lighting_constants_[i].init(device_.Get());

			depth_constant_resources_[i].init(
				depth_constants_[i].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
			gbuffer_constant_resources_[i].init(
				gbuffer_constants_[i].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
			lighting_constant_resources_[i].init(
				lighting_constants_[i].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
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
				"reset command allocator on Donut neutral resource creation");
			util::Utils::throw_if_failed(
				command_list_->Reset(command_allocator_[frame_index_].Get(), nullptr),
				"reset command list on Donut neutral resource creation");
			neutral_resources_.init(device_.Get(), command_list_.Get(), used_upload_heaps);
			util::Utils::throw_if_failed(
				command_list_->Close(),
				"close command list on Donut neutral resource creation");
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

		PassDonutGBufferResources gbuffer{};
		gbuffer.frame_manager = &resource_manager_frame_;
		gbuffer.sampler_manager = &resource_manager_sampler_;
		gbuffer.shader_manager = &resource_manager_shader_;
		gbuffer.depth = &depth_stencil_buffer_;
		for (UINT i = 0; i < GBUFFER_COUNT; ++i)
			gbuffer.gbuffers[i] = &gbuffers_[i];
		for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i)
			gbuffer.constant_buffers[i] = &gbuffer_constant_resources_[i];
		gbuffer.scene = scene_gpu_.get();

		if (do_prepass_) {
			PassDonutDepthPreResources depth{};
			depth.frame_manager = &resource_manager_frame_;
			depth.sampler_manager = &resource_manager_sampler_;
			depth.shader_manager = &resource_manager_shader_;
			depth.depth = &depth_stencil_buffer_;
			for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i)
				depth.constant_buffers[i] = &depth_constant_resources_[i];
			depth.scene = scene_gpu_.get();
			pass_depth_pre_.init(device_.Get(), program_argument_, depth);
		}

		pass_gbuffer_.init(device_.Get(), program_argument_, gbuffer, do_prepass_);

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

	void RendererDonutDeferred::render_prepare_donut_() {
		const scene::DonutPlanarViewConstants current_view =
			DonutFrameConstantsBuilder::make_planar_view(
				camera_, width_, height_);
		const scene::DonutPlanarViewConstants previous_view =
			has_previous_view_constants_ ?
			previous_view_constants_ :
			current_view;

		depth_constants_[frame_index_].buffer =
			DonutFrameConstantsBuilder::make_depth_constants(current_view);
		depth_constants_[frame_index_].update();

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

	void RendererDonutDeferred::render_record_() {
		const UINT geometry_slot = do_prepass_ ? 2 : 1;
		const UINT lighting_slot = geometry_slot + 1;
		const UINT tonemap_slot = lighting_slot + 1;
		record_render_instance_upload_(command_list_.Get());

		if (do_prepass_) {
			frame_time_.start_timestamp(
				command_list_.Get(), frame_index_, 1);
			pass_depth_pre_.render(
				command_list_.Get(), frame_index_, viewport_, scissor_rect_);
			frame_time_.end_timestamp(
				command_list_.Get(), frame_index_, 1);
		}

		frame_time_.start_timestamp(command_list_.Get(), frame_index_, geometry_slot);
		pass_gbuffer_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
		frame_time_.end_timestamp(command_list_.Get(), frame_index_, geometry_slot);

		frame_time_.start_timestamp(command_list_.Get(), frame_index_, lighting_slot);
		pass_lighting_.render(command_list_.Get(), frame_index_, width_, height_);
		frame_time_.end_timestamp(command_list_.Get(), frame_index_, lighting_slot);

		frame_time_.start_timestamp(command_list_.Get(), frame_index_, tonemap_slot);
		pass_tonemap_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
		frame_time_.end_timestamp(command_list_.Get(), frame_index_, tonemap_slot);
	}
}
