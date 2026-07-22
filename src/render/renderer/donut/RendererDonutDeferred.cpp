#include "render/renderer/donut/RendererDonutDeferred.h"

#include <algorithm>
#include <cstdint>

#include "util/Constants.h"
#include "util/RenderConstants.h"
#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"
#include "engine/ResourceViewBuilder.h"
#include "engine/RootSignatureBuilder.h"

namespace rndr {

	RendererDonutDeferred::RendererDonutDeferred(bool do_prepass)
		: do_prepass_(do_prepass) {}

	void RendererDonutDeferred::init2_() {
		program_result_.renderer_name =
			do_prepass_ ? "DonutDeferredPrepass" : "DonutDeferred";

		const UINT geometry_slot = do_prepass_ ? 2 : 1;
		const UINT lighting_slot = geometry_slot + 1;
		if (do_prepass_)
			program_result_.pass_names[DEPTH_PREPASS_SLOT] = "depth_prepass";
		program_result_.pass_names[geometry_slot] = "geometry";
		program_result_.pass_names[lighting_slot] = "lighting";

		this->init_constant_buffers_();
		this->init_gbuffers_();
		this->init_lighting_fallbacks_();

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
			depth.shader_manager = &resource_manager_shader_;
			depth.depth = &depth_stencil_buffer_;
			for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i)
				depth.constant_buffers[i] = &depth_constant_resources_[i];
			depth.scene = scene_gpu_.get();
			pass_depth_pre_.init(device_.Get(), program_argument_, depth, true);
		}

		pass_gbuffer_.init(device_.Get(), program_argument_, gbuffer, do_prepass_);

		PassDonutDeferredLightingResources lighting{};
		lighting.shader_manager = &resource_manager_shader_;
		lighting.sampler_manager = &resource_manager_sampler_;
		for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i)
			lighting.constant_buffers[i] = &lighting_constant_resources_[i];
		lighting.buf_shadow_map = &fallback_shadow_map_;
		lighting.buf_diffuse_light_probe = &fallback_diffuse_light_probe_;
		lighting.buf_specular_light_probe = &fallback_specular_light_probe_;
		lighting.buf_env_brdf = &fallback_env_brdf_;
		lighting.depth = &depth_stencil_buffer_;
		for (UINT i = 0; i < GBUFFER_COUNT; ++i)
			lighting.gbuffers[i] = &gbuffers_[i];
		lighting.buf_ibl_diffuse = &fallback_ibl_diffuse_;
		lighting.buf_ibl_specular = &fallback_ibl_specular_;
		lighting.buf_shadow = &fallback_shadow_channels_;
		lighting.buf_ao = &fallback_ambient_occlusion_;
		lighting.uav_output = &hdr_color_buffer_;
		pass_lighting_.init(device_.Get(), program_argument_, lighting);

		PassDonutTonemapResources tonemap{};
		pass_tonemap_.init(device_.Get(), program_argument_, tonemap);
	}

	void RendererDonutDeferred::render_prepare_() {
		const float width = static_cast<float>(width_);
		const float height = static_cast<float>(height_);
		const DirectX::XMMATRIX world_to_view = camera_.get_mat_view();
		const DirectX::XMMATRIX view_to_clip = camera_.get_mat_proj(width_, height_);
		const DirectX::XMMATRIX world_to_clip = world_to_view * view_to_clip;
		const DirectX::XMMATRIX clip_to_view =
			DirectX::XMMatrixInverse(nullptr, view_to_clip);
		const DirectX::XMMATRIX view_to_world =
			DirectX::XMMatrixInverse(nullptr, world_to_view);
		const DirectX::XMMATRIX clip_to_world =
			DirectX::XMMatrixInverse(nullptr, world_to_clip);
		const CameraPose camera_pose = camera_.get_pose();

		auto write_planar_view = [&](DonutPlanarViewConstants& view) {
			DirectX::XMStoreFloat4x4(&view.mat_world_to_view, world_to_view);
			DirectX::XMStoreFloat4x4(&view.mat_view_to_clip, view_to_clip);
			DirectX::XMStoreFloat4x4(&view.mat_world_to_clip, world_to_clip);
			DirectX::XMStoreFloat4x4(&view.mat_clip_to_view, clip_to_view);
			DirectX::XMStoreFloat4x4(&view.mat_view_to_world, view_to_world);
			DirectX::XMStoreFloat4x4(&view.mat_clip_to_world, clip_to_world);
			DirectX::XMStoreFloat4x4(&view.mat_view_to_clip_no_offset, view_to_clip);
			DirectX::XMStoreFloat4x4(&view.mat_world_to_clip_no_offset, world_to_clip);
			DirectX::XMStoreFloat4x4(&view.mat_clip_to_view_no_offset, clip_to_view);
			DirectX::XMStoreFloat4x4(&view.mat_clip_to_world_no_offset, clip_to_world);

			view.viewport_origin = { 0.0f, 0.0f };
			view.viewport_size = { width, height };
			view.viewport_size_inv = { 1.0f / width, 1.0f / height };
			view.pixel_offset = { 0.0f, 0.0f };
			view.clip_to_window_scale = { width * 0.5f, height * -0.5f };
			view.clip_to_window_bias = { width * 0.5f, height * 0.5f };
			view.window_to_clip_scale = { 2.0f / width, -2.0f / height };
			view.window_to_clip_bias = { -1.0f, 1.0f };
			view.camera_direction_or_position = {
				camera_pose.position.x,
				camera_pose.position.y,
				camera_pose.position.z,
				1.0f
			};
		};

		depth_constants_[frame_index_].buffer = DonutDepthPassConstants{};
		DirectX::XMStoreFloat4x4(
			&depth_constants_[frame_index_].buffer.mat_world_to_clip,
			world_to_clip);
		depth_constants_[frame_index_].update();

		gbuffer_constants_[frame_index_].buffer = DonutGBufferFillConstants{};
		write_planar_view(gbuffer_constants_[frame_index_].buffer.view);
		gbuffer_constants_[frame_index_].buffer.view_prev =
			gbuffer_constants_[frame_index_].buffer.view;
		gbuffer_constants_[frame_index_].update();

		lighting_constants_[frame_index_].buffer = DonutDeferredLightingConstants{};
		write_planar_view(lighting_constants_[frame_index_].buffer.view);
		lighting_constants_[frame_index_].buffer.shadow_map_texture_size = { 1.0f, 1.0f };
		lighting_constants_[frame_index_].buffer.ambient_color_top =
			{ 0.8f, 0.8f, 0.8f, 1.0f };
		lighting_constants_[frame_index_].buffer.ambient_color_bottom =
			{ 0.8f, 0.8f, 0.8f, 1.0f };
		for (UINT y = 0; y < 4; ++y)
			lighting_constants_[frame_index_].buffer.noise_pattern[y] =
				{ 0.0f, 0.0f, 0.0f, 0.0f };
		for (UINT i = 0; i < DEFERRED_MAX_LIGHTS; ++i) {
			lighting_constants_[frame_index_].buffer.lights[i].shadow_cascades =
				{ -1, -1, -1, -1 };
			lighting_constants_[frame_index_].buffer.lights[i].per_object_shadows =
				{ -1, -1, -1, -1 };
			lighting_constants_[frame_index_].buffer.lights[i].shadow_channel =
				{ -1, -1, -1, -1 };
		}
		lighting_constants_[frame_index_].update();
	}

	void RendererDonutDeferred::render_record_() {
		const UINT geometry_slot = do_prepass_ ? 2 : 1;
		const UINT lighting_slot = geometry_slot + 1;

		if (do_prepass_) {
			frame_time_.start_timestamp(
				command_list_.Get(), frame_index_, DEPTH_PREPASS_SLOT);
			pass_depth_pre_.render(
				command_list_.Get(), frame_index_, viewport_, scissor_rect_);
			frame_time_.end_timestamp(
				command_list_.Get(), frame_index_, DEPTH_PREPASS_SLOT);
		}

		frame_time_.start_timestamp(command_list_.Get(), frame_index_, geometry_slot);
		pass_gbuffer_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
		frame_time_.end_timestamp(command_list_.Get(), frame_index_, geometry_slot);

		frame_time_.start_timestamp(command_list_.Get(), frame_index_, lighting_slot);
		pass_lighting_.render(command_list_.Get(), frame_index_, width_, height_);
		pass_tonemap_.render(command_list_.Get(), frame_index_, viewport_, scissor_rect_);
		frame_time_.end_timestamp(command_list_.Get(), frame_index_, lighting_slot);
	}

	void RendererDonutDeferred::init_constant_buffers_() {
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
	}

	void RendererDonutDeferred::init_gbuffers_() {
		gbuffers_.reserve(GBUFFER_COUNT);
		for (UINT i = 0; i < GBUFFER_COUNT; ++i) {
			D3D12_CLEAR_VALUE clear{};
			clear.Format = util::RenderConstants::DONUT_GBUFFER_FORMATS[i];
			clear.Color[0] = 0.0f;
			clear.Color[1] = 0.0f;
			clear.Color[2] = 0.0f;
			clear.Color[3] = 0.0f;

			auto gbuffer = dxutl::create_texture2d(
				device_.Get(),
				width_,
				height_,
				util::RenderConstants::DONUT_GBUFFER_FORMATS[i],
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
				&clear);
			gbuffers_.emplace_back();
			gbuffers_.back().init(
				gbuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}

	void RendererDonutDeferred::init_lighting_fallbacks_() {
		fallback_shadow_map_.init(
			dxutl::create_texture2d_array(
				device_.Get(), 1, 1, 1, DXGI_FORMAT_R32_FLOAT,
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE).Get(),
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		fallback_diffuse_light_probe_.init(
			dxutl::create_texture2d_array(
				device_.Get(), 1, 1, 6, DXGI_FORMAT_R16G16B16A16_FLOAT,
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE).Get(),
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		fallback_specular_light_probe_.init(
			dxutl::create_texture2d_array(
				device_.Get(), 1, 1, 6, DXGI_FORMAT_R16G16B16A16_FLOAT,
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE).Get(),
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		fallback_env_brdf_.init(
			dxutl::create_texture2d(
				device_.Get(), 1, 1, DXGI_FORMAT_R16G16_FLOAT,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE).Get(),
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		fallback_ibl_diffuse_.init(
			dxutl::create_texture2d(
				device_.Get(), 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE).Get(),
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		fallback_ibl_specular_.init(
			dxutl::create_texture2d(
				device_.Get(), 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE).Get(),
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		fallback_shadow_channels_.init(
			dxutl::create_texture2d(
				device_.Get(), 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE).Get(),
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		fallback_ambient_occlusion_.init(
			dxutl::create_texture2d(
				device_.Get(), 1, 1, DXGI_FORMAT_R8_UNORM,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE).Get(),
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		fallback_color_lut_.init(
			dxutl::create_texture2d(
				device_.Get(), 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE).Get(),
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

		const std::uint32_t zero_exposure = 0;
		auto exposure = dxutl::create_buffer(
			device_.Get(),
			sizeof(zero_exposure),
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ);
		dxutl::copy_to_upload_buffer(
			exposure.Get(), &zero_exposure, sizeof(zero_exposure));
		exposure_buffer_.init(exposure.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);
	}
}
