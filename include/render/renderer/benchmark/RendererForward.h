#pragma once

#include "render/renderer/benchmark/RendererBenchmark.h"
#include "render/pass/benchmark/PassDepthPre.h"
#include "render/pass/benchmark/PassForward.h"

namespace rndr {

	class RendererForward : public RendererBenchmark {
	public:
		explicit RendererForward(bool do_prepass);
		~RendererForward() override = default;

	private:
		void init2_() override;
		void render_record_() override;

		bool do_prepass_ = false;
		PassDepthPre pass_depth_pre_;
		PassForward pass_forward_;
	};
}
