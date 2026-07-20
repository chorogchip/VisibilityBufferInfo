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
		void record_render_commands_() override;
		void init_programresult_(util::ProgramResult& result) override;
		void init_renderer_resources_() override;

		void init_passes_() override;

		bool do_prepass_ = false;
		PassDepthPre pass_depth_pre_;
		PassGBuffer pass_gbuffer_;
		PassDeferredLighting pass_lighting_;
		std::vector<eng::GPUResource> gbuffers_;
	};
}
