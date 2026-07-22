#pragma once

#include <cstdint>
#include <DirectXMath.h>

namespace scene {

	struct DonutPlanarViewConstants {
		DirectX::XMFLOAT4X4 mat_world_to_view{};
		DirectX::XMFLOAT4X4 mat_view_to_clip{};
		DirectX::XMFLOAT4X4 mat_world_to_clip{};
		DirectX::XMFLOAT4X4 mat_clip_to_view{};
		DirectX::XMFLOAT4X4 mat_view_to_world{};
		DirectX::XMFLOAT4X4 mat_clip_to_world{};
		DirectX::XMFLOAT4X4 mat_view_to_clip_no_offset{};
		DirectX::XMFLOAT4X4 mat_world_to_clip_no_offset{};
		DirectX::XMFLOAT4X4 mat_clip_to_view_no_offset{};
		DirectX::XMFLOAT4X4 mat_clip_to_world_no_offset{};
		DirectX::XMFLOAT2 viewport_origin{};
		DirectX::XMFLOAT2 viewport_size{};
		DirectX::XMFLOAT2 viewport_size_inv{};
		DirectX::XMFLOAT2 pixel_offset{};
		DirectX::XMFLOAT2 clip_to_window_scale{};
		DirectX::XMFLOAT2 clip_to_window_bias{};
		DirectX::XMFLOAT2 window_to_clip_scale{};
		DirectX::XMFLOAT2 window_to_clip_bias{};
		DirectX::XMFLOAT4 camera_direction_or_position{};
	};

	struct DonutDepthPassConstants {
		DirectX::XMFLOAT4X4 mat_world_to_clip{};
	};

	struct DonutGBufferFillConstants {
		DonutPlanarViewConstants view{};
		DonutPlanarViewConstants view_prev{};
	};

	struct DonutShadowConstants {
		DirectX::XMFLOAT4X4 mat_world_to_uvzw_shadow{};
		DirectX::XMFLOAT2 shadow_fade_scale{};
		DirectX::XMFLOAT2 shadow_fade_bias{};
		DirectX::XMFLOAT2 shadow_map_center_uv{};
		float shadow_falloff_distance = 0.0f;
		std::int32_t shadow_map_array_index = 0;
		DirectX::XMFLOAT2 shadow_map_size_texels{};
		DirectX::XMFLOAT2 shadow_map_size_texels_inv{};
	};

	struct DonutLightConstants {
		DirectX::XMFLOAT3 direction{};
		std::int32_t light_type = 0;
		DirectX::XMFLOAT3 position{};
		float radius = 0.0f;
		DirectX::XMFLOAT3 color{};
		float intensity = 0.0f;
		float angular_size_or_inv_range = 0.0f;
		float inner_angle = 0.0f;
		float outer_angle = 0.0f;
		float out_of_bounds_shadow = 0.0f;
		DirectX::XMINT4 shadow_cascades{};
		DirectX::XMINT4 per_object_shadows{};
		DirectX::XMINT4 shadow_channel{};
	};

	struct DonutLightProbeConstants {
		float diffuse_scale = 0.0f;
		float specular_scale = 0.0f;
		float mip_levels = 0.0f;
		float padding1 = 0.0f;
		std::uint32_t diffuse_array_index = 0;
		std::uint32_t specular_array_index = 0;
		std::uint32_t padding2[2]{};
		DirectX::XMFLOAT4 frustum_planes[6]{};
	};

	struct DonutDeferredLightingConstants {
		static constexpr UINT DEFERRED_MAX_LIGHTS = 16;
		static constexpr UINT DEFERRED_MAX_SHADOWS = 16;
		static constexpr UINT DEFERRED_MAX_LIGHT_PROBES = 16;

		DonutPlanarViewConstants view{};
		DirectX::XMFLOAT2 shadow_map_texture_size{};
		std::int32_t enable_ambient_occlusion = 0;
		std::int32_t padding = 0;
		DirectX::XMFLOAT4 ambient_color_top{};
		DirectX::XMFLOAT4 ambient_color_bottom{};
		std::uint32_t num_lights = 0;
		std::uint32_t num_light_probes = 0;
		float indirect_diffuse_scale = 0.0f;
		float indirect_specular_scale = 0.0f;
		DirectX::XMFLOAT2 random_offset{};
		DirectX::XMFLOAT2 padding2{};
		DirectX::XMFLOAT4 noise_pattern[4]{};
		DonutLightConstants lights[DEFERRED_MAX_LIGHTS]{};
		DonutShadowConstants shadows[DEFERRED_MAX_SHADOWS]{};
		DonutLightProbeConstants light_probes[DEFERRED_MAX_LIGHT_PROBES]{};
	};

	static_assert(sizeof(DonutPlanarViewConstants) == 720);
	static_assert(sizeof(DonutDepthPassConstants) == 64);
	static_assert(sizeof(DonutGBufferFillConstants) == 1440);
	static_assert(sizeof(DonutLightConstants) == 112);
	static_assert(sizeof(DonutShadowConstants) == 112);
	static_assert(sizeof(DonutLightProbeConstants) == 128);
	static_assert(sizeof(DonutDeferredLightingConstants) == 6496);
}