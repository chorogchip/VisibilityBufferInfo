#pragma once

#include <vector>

#include "engine/GPUResource.h"
#include "render/renderer/benchmark/RendererBenchmark.h"
#include "render/pass/benchmark/PassDeferredLighting.h"
#include "render/pass/benchmark/PassDepthPre.h"
#include "render/pass/benchmark/PassGBuffer.h"

namespace rndr {

	class RendererDeferred : public RendererBenchmark {
	public:
		explicit RendererDeferred(bool do_prepass);
		~RendererDeferred() override = default;

	private:
		void init2_() override;
		void render_record_() override;

		bool do_prepass_ = false;
		PassDepthPre pass_depth_pre_;
		PassGBuffer pass_gbuffer_;
		PassDeferredLighting pass_lighting_;
		std::vector<eng::GPUResource> gbuffers_;
	};
}
