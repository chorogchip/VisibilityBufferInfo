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
		update_visible_draws_();

		auto write_planar_view = [&](scene::DonutPlanarViewConstants& view) {
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

		depth_constants_[frame_index_].buffer =
			scene::DonutDepthPassConstants{};
		DirectX::XMStoreFloat4x4(
			&depth_constants_[frame_index_].buffer.mat_world_to_clip,
			world_to_clip);
		depth_constants_[frame_index_].update();

		gbuffer_constants_[frame_index_].buffer =
			scene::DonutGBufferFillConstants{};
		write_planar_view(gbuffer_constants_[frame_index_].buffer.view);
		gbuffer_constants_[frame_index_].buffer.view_prev =
			gbuffer_constants_[frame_index_].buffer.view;
		gbuffer_constants_[frame_index_].update();

		lighting_constants_[frame_index_].buffer =
			scene::DonutDeferredLightingConstants{};
		write_planar_view(lighting_constants_[frame_index_].buffer.view);
		lighting_constants_[frame_index_].buffer.shadow_map_texture_size = { 1.0f, 1.0f };
		lighting_constants_[frame_index_].buffer.ambient_color_top =
		{ 0.05f, 0.05f, 0.05f, 0.0f };
		lighting_constants_[frame_index_].buffer.ambient_color_bottom =
		{ 0.02f, 0.02f, 0.02f, 0.0f };
		lighting_constants_[frame_index_].buffer.indirect_diffuse_scale = 1.0f;
		lighting_constants_[frame_index_].buffer.indirect_specular_scale = 1.0f;
		lighting_constants_[frame_index_].buffer.noise_pattern[0] =
		{ 0.059f, 0.529f, 0.176f, 0.647f };
		lighting_constants_[frame_index_].buffer.noise_pattern[1] =
		{ 0.765f, 0.294f, 0.882f, 0.412f };
		lighting_constants_[frame_index_].buffer.noise_pattern[2] =
		{ 0.235f, 0.706f, 0.118f, 0.588f };
		lighting_constants_[frame_index_].buffer.noise_pattern[3] =
		{ 0.941f, 0.471f, 0.824f, 0.353f };
		for (UINT i = 0; i < scene::DonutDeferredLightingConstants::DEFERRED_MAX_LIGHTS; ++i) {
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
