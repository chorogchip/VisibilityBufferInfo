#pragma once

#include <vector>

#include "engine/GPUResource.h"
#include "render/renderer/donut/RendererDonut.h"
#include "render/pass/donut/PassDonutDepthPre.h"
#include "render/pass/donut/PassDonutGBuffer.h"
#include "render/pass/donut/PassDonutDeferredLighting.h"

namespace rndr {

	class RendererDonutDeferred : public RendererDonut {
	public:
		explicit RendererDonutDeferred(bool do_prepass);
		~RendererDonutDeferred() override = default;

	private:
		void init2_() override;
		void render_record_() override;

		bool do_prepass_ = false;
		PassDonutDepthPre pass_depth_pre_;
		PassDonutGBuffer pass_gbuffer_;
		PassDonutDeferredLighting pass_lighting_;
		std::vector<eng::GPUResource> gbuffers_;
	};
}
