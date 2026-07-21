#pragma once

#include "engine/GPUResource.h"
#include "render/renderer/benchmark/RendererBenchmark.h"
#include "render/pass/benchmark/PassVisibility.h"
#include "render/pass/benchmark/PassVisBufResolve.h"

namespace rndr {

	class RendererVisBuf : public RendererBenchmark {
	public:
		RendererVisBuf() = default;
		~RendererVisBuf() override = default;

	private:
		void init2_() override;
		void render_record_() override;

		eng::GPUResource vis_buffer_;
		PassVisibility pass_visibility_;
		PassVisBufResolve pass_resolve_;
	};
}
