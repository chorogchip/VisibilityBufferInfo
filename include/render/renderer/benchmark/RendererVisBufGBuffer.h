#pragma once

#include <vector>

#include "engine/GPUResource.h"
#include "render/renderer/benchmark/RendererBenchmark.h"
#include "render/pass/benchmark/PassVisibility.h"
#include "render/pass/benchmark/PassVisBufGBuffer.h"
#include "render/pass/benchmark/PassDeferredLighting.h"

namespace rndr {

	class RendererVisBufGBuffer : public RendererBenchmark {
	public:
		RendererVisBufGBuffer() = default;
		~RendererVisBufGBuffer() override = default;

	private:
		void init2_() override;
		void render_record_() override;

		eng::GPUResource vis_buffer_;
		std::vector<eng::GPUResource> gbuffers_;
		PassVisibility pass_visibility_;
		PassVisBufGBuffer pass_gbuffer_;
		PassDeferredLighting pass_lighting_;
	};
}
