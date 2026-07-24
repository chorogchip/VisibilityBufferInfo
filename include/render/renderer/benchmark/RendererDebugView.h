#pragma once

#include "render/renderer/benchmark/RendererBenchmark.h"
#include "render/pass/benchmark/PassDebugView.h"

namespace rndr {

	class RendererDebugView : public RendererBenchmark {
	public:
		explicit RendererDebugView();
		~RendererDebugView() override = default;

	private:
		void init2_() override;
		void render_record_() override;

		PassDebugView pass_debug_view_;
	};
}
