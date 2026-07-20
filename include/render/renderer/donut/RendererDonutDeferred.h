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
		void record_render_commands_() override;
		void init_programresult_(util::ProgramResult& result) override;
		void init_renderer_resources_() override;

		void init_passes_() override;

		bool do_prepass_ = false;
		PassDonutDepthPre pass_depth_pre_;
		PassDonutGBuffer pass_gbuffer_;
		PassDonutDeferredLighting pass_lighting_;
		std::vector<eng::GPUResource> gbuffers_;
	};
}
