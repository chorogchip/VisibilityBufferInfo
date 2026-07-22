#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <DirectXMath.h>

#include "dx_util/UploadConstBuf.h"
#include "engine/GPUResource.h"
#include "engine/GraphicsPipeline.h"
#include "render/renderer/donut/RendererDonut.h"
#include "scene/donut/DonutRenderConstants.h"
#include "render/pass/donut/PassDonutDepthPre.h"
#include "render/pass/donut/PassDonutGBuffer.h"
#include "render/pass/donut/PassDonutDeferredLighting.h"
#include "render/pass/donut/PassDonutTonemap.h"

namespace rndr {

	class RendererDonutDeferred : public RendererDonut {
	public:
		explicit RendererDonutDeferred(bool do_prepass);
		~RendererDonutDeferred() override = default;

	private:
		void init2_() override;
		void render_prepare_() override;
		void render_record_() override;

		static constexpr UINT GBUFFER_COUNT = 4;

		bool do_prepass_ = false;
		PassDonutDepthPre pass_depth_pre_;
		PassDonutGBuffer pass_gbuffer_;
		PassDonutDeferredLighting pass_lighting_;
		PassDonutTonemap pass_tonemap_;

		eng::GPUResource fallback_shadow_map_;
		eng::GPUResource fallback_diffuse_light_probe_;
		eng::GPUResource fallback_specular_light_probe_;
		eng::GPUResource fallback_env_brdf_;

		std::array<eng::GPUResource, GBUFFER_COUNT> gbuffers_;

		eng::GPUResource fallback_ibl_diffuse_;
		eng::GPUResource fallback_ibl_specular_;
		eng::GPUResource fallback_shadow_channels_;
		eng::GPUResource fallback_ambient_occlusion_;

		eng::GPUResource exposure_buffer_;
		eng::GPUResource hdr_color_buffer_;

		std::array<dxutl::UploadConstBuf<scene::DonutDepthPassConstants>,
			util::Constants::FRAME_COUNT> depth_constants_;
		std::array<dxutl::UploadConstBuf<scene::DonutGBufferFillConstants>,
			util::Constants::FRAME_COUNT> gbuffer_constants_;
		std::array<dxutl::UploadConstBuf<scene::DonutDeferredLightingConstants>,
			util::Constants::FRAME_COUNT> lighting_constants_;
		std::array<eng::GPUResource, util::Constants::FRAME_COUNT>
			depth_constant_resources_;
		std::array<eng::GPUResource, util::Constants::FRAME_COUNT>
			gbuffer_constant_resources_;
		std::array<eng::GPUResource, util::Constants::FRAME_COUNT>
			lighting_constant_resources_;
	};
}
