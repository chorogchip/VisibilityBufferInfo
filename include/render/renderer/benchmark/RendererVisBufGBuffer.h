#pragma once

#include <vector>

#include "engine/GPUResource.h"
#include "render/renderer/benchmark/RendererBenchmark.h"
#include "render/pass/PassVisibility.h"
#include "render/pass/PassVisBufGBuffer.h"
#include "render/pass/PassDeferredLighting.h"

namespace rndr {

	class RendererVisBufGBuffer : public RendererBenchmark {
	public:
		RendererVisBufGBuffer() = default;
		~RendererVisBufGBuffer() override = default;

	private:
		void record_render_commands_() override;
		void init_programresult_(util::ProgramResult& result) override;
		void init_renderer_resources_() override;

		void init_passes_() override;

		eng::GPUResource vis_buffer_;
		std::vector<eng::GPUResource> gbuffers_;
		PassVisibility pass_visibility_;
		PassVisBufGBuffer pass_gbuffer_;
		PassDeferredLighting pass_lighting_;
	};
}
