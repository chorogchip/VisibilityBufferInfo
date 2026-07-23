#include "render/renderer/donut/DonutFrameConstantsBuilder.h"

#include <cmath>

#include <DirectXMath.h>

#include "donut/donut_light_types.h"
#include "render/camera/Camera.h"
#include "util/Logger.h"

namespace rndr {

	namespace {

		bool is_finite(float value) {
			return std::isfinite(value);
		}

		void assert_finite_matrix(
			const DirectX::XMFLOAT4X4& matrix,
			const char* name) {
			const float* values = &matrix._11;
			for (int i = 0; i < 16; ++i) {
				if (!is_finite(values[i])) {
					util::Logger::g_logger << "non-finite Donut matrix: " << name << '\n';
					util::Logger::g_logger.assert_with_log(
						false, "Donut view matrix contains a non-finite value");
				}
			}
		}

		void assert_finite_view(const scene::DonutPlanarViewConstants& view) {
			assert_finite_matrix(view.mat_world_to_view, "mat_world_to_view");
			assert_finite_matrix(view.mat_view_to_clip, "mat_view_to_clip");
			assert_finite_matrix(view.mat_world_to_clip, "mat_world_to_clip");
			assert_finite_matrix(view.mat_clip_to_view, "mat_clip_to_view");
			assert_finite_matrix(view.mat_view_to_world, "mat_view_to_world");
			assert_finite_matrix(view.mat_clip_to_world, "mat_clip_to_world");
		}

		void disable_light_shadows(scene::DonutLightConstants& light) {
			light.shadow_cascades = { -1, -1, -1, -1 };
			light.per_object_shadows = { -1, -1, -1, -1 };
			light.shadow_channel = { -1, -1, -1, -1 };
		}

	}

	scene::DonutPlanarViewConstants DonutFrameConstantsBuilder::make_planar_view(
		const Camera& camera,
		std::uint32_t width,
		std::uint32_t height) {

		util::Logger::g_logger.assert_with_log(width > 0, "Donut view width must be greater than 0");
		util::Logger::g_logger.assert_with_log(height > 0, "Donut view height must be greater than 0");

		const float width_f = static_cast<float>(width);
		const float height_f = static_cast<float>(height);
		const DirectX::XMMATRIX world_to_view = camera.get_mat_view();
		const DirectX::XMMATRIX view_to_clip = camera.get_mat_proj(width, height);
		const DirectX::XMMATRIX world_to_clip = world_to_view * view_to_clip;
		const DirectX::XMMATRIX clip_to_view =
			DirectX::XMMatrixInverse(nullptr, view_to_clip);
		const DirectX::XMMATRIX view_to_world =
			DirectX::XMMatrixInverse(nullptr, world_to_view);
		const DirectX::XMMATRIX clip_to_world =
			DirectX::XMMatrixInverse(nullptr, world_to_clip);
		const CameraPose camera_pose = camera.get_pose();

		scene::DonutPlanarViewConstants view{};
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
		view.viewport_size = { width_f, height_f };
		view.viewport_size_inv = { 1.0f / width_f, 1.0f / height_f };
		view.pixel_offset = { 0.0f, 0.0f };
		view.clip_to_window_scale = { width_f * 0.5f, height_f * -0.5f };
		view.clip_to_window_bias = { width_f * 0.5f, height_f * 0.5f };
		view.window_to_clip_scale = { 2.0f / width_f, -2.0f / height_f };
		view.window_to_clip_bias = { -1.0f, 1.0f };
		view.camera_direction_or_position = {
			camera_pose.position.x,
			camera_pose.position.y,
			camera_pose.position.z,
			1.0f
		};

		assert_finite_view(view);
		return view;
	}

	scene::DonutDepthPassConstants DonutFrameConstantsBuilder::make_depth_constants(
		const scene::DonutPlanarViewConstants& view) {

		scene::DonutDepthPassConstants constants{};
		constants.mat_world_to_clip = view.mat_world_to_clip;
		return constants;
	}

	scene::DonutGBufferFillConstants DonutFrameConstantsBuilder::make_gbuffer_constants(
		const scene::DonutPlanarViewConstants& current_view,
		const scene::DonutPlanarViewConstants& previous_view) {

		scene::DonutGBufferFillConstants constants{};
		constants.view = current_view;
		constants.view_prev = previous_view;
		return constants;
	}

	scene::DonutDeferredLightingConstants
		DonutFrameConstantsBuilder::make_deferred_lighting_constants(
			const scene::DonutPlanarViewConstants& view,
			std::uint32_t frame_index) {

		scene::DonutDeferredLightingConstants constants{};
		constants.view = view;
		constants.shadow_map_texture_size = { 1.0f, 1.0f };
		constants.enable_ambient_occlusion = 0;
		constants.ambient_color_top = { 0.05f, 0.05f, 0.05f, 0.0f };
		constants.ambient_color_bottom = { 0.02f, 0.02f, 0.02f, 0.0f };
		constants.num_lights = 1;
		constants.num_light_probes = 0;
		constants.indirect_diffuse_scale = 0.0f;
		constants.indirect_specular_scale = 0.0f;
		constants.random_offset = {
			static_cast<float>((frame_index * 2u) & 3u),
			static_cast<float>((frame_index / 2u) & 3u)
		};

		constants.noise_pattern[0] = { 0.059f, 0.529f, 0.176f, 0.647f };
		constants.noise_pattern[1] = { 0.765f, 0.294f, 0.882f, 0.412f };
		constants.noise_pattern[2] = { 0.235f, 0.706f, 0.118f, 0.588f };
		constants.noise_pattern[3] = { 0.941f, 0.471f, 0.824f, 0.353f };

		for (scene::DonutLightConstants& light : constants.lights) {
			disable_light_shadows(light);
		}

		scene::DonutLightConstants& key_light = constants.lights[0];
		key_light.direction = { 0.35f, -0.75f, 0.55f };
		key_light.light_type = LightType_Directional;
		key_light.color = { 1.0f, 0.96f, 0.90f };
		key_light.intensity = 4.0f;
		key_light.angular_size_or_inv_range = 0.0093f;
		disable_light_shadows(key_light);

		return constants;
	}
}
