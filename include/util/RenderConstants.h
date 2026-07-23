#pragma once

#include <array>

#include <d3d12.h>

namespace util {

	class RenderConstants {

	public:
		static inline constexpr float CLEAR_COLOR[] = { 0.1f, 0.1f, 0.15f, 1.0f };
		static inline constexpr DXGI_FORMAT DEPTH_STENCIL_FORMAT = DXGI_FORMAT_D32_FLOAT;
		static inline constexpr DXGI_FORMAT DONUT_DEPTH_RESOURCE_FORMAT = DXGI_FORMAT_R32_TYPELESS;
		static inline constexpr DXGI_FORMAT DONUT_DEPTH_DSV_FORMAT = DXGI_FORMAT_D32_FLOAT;
		static inline constexpr DXGI_FORMAT DONUT_DEPTH_SRV_FORMAT = DXGI_FORMAT_R32_FLOAT;
		// Donut G-buffer ABI. Shaders must preserve the channel contract:
		// RT0 diffuseAlbedo.rgb + opacity, RT1 specularF0.rgb + occlusion,
		// RT2 worldNormal.xyz + roughness, RT3 emissive.rgb + unused.
		static inline constexpr std::array<DXGI_FORMAT, 4> DONUT_GBUFFER_FORMATS = {
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DXGI_FORMAT_R16G16B16A16_SNORM,
			DXGI_FORMAT_R16G16B16A16_FLOAT,
		};
	};

	static_assert(RenderConstants::DONUT_GBUFFER_FORMATS.size() == 4);
	static_assert(RenderConstants::DONUT_GBUFFER_FORMATS[0] == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	static_assert(RenderConstants::DONUT_GBUFFER_FORMATS[1] == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	static_assert(RenderConstants::DONUT_GBUFFER_FORMATS[2] == DXGI_FORMAT_R16G16B16A16_SNORM);
	static_assert(RenderConstants::DONUT_GBUFFER_FORMATS[3] == DXGI_FORMAT_R16G16B16A16_FLOAT);
}
