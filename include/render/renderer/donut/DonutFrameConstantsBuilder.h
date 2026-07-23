#pragma once

#include <cstdint>

#include "scene/donut/DonutRenderConstants.h"

namespace rndr {

	class Camera;

	class DonutFrameConstantsBuilder {
	public:
		static scene::DonutPlanarViewConstants make_planar_view(
			const Camera& camera,
			std::uint32_t width,
			std::uint32_t height);

		static scene::DonutDepthPassConstants make_depth_constants(
			const scene::DonutPlanarViewConstants& view);

		static scene::DonutGBufferFillConstants make_gbuffer_constants(
			const scene::DonutPlanarViewConstants& current_view,
			const scene::DonutPlanarViewConstants& previous_view);

		static scene::DonutDeferredLightingConstants make_deferred_lighting_constants(
			const scene::DonutPlanarViewConstants& view,
			std::uint32_t frame_index);
	};
}
