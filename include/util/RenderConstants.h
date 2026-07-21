#pragma once

#include <d3d12.h>

namespace util {

	class RenderConstants {

	public:
		static inline constexpr float CLEAR_COLOR[] = { 0.1f, 0.1f, 0.15f, 1.0f };
		static inline constexpr DXGI_FORMAT DEPTH_STENCIL_FORMAT = DXGI_FORMAT_D32_FLOAT;
	};
}