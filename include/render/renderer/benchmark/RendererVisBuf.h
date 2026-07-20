#pragma once

#include <vector>

#include "engine/GPUResource.h"
#include "render/renderer/benchmark/RendererBenchmark.h"
#include "render/pass/PassVisibility.h"
#include "render/pass/PassVisBufResolve.h"

namespace rndr {

	class RendererVisBuf : public RendererBenchmark {
	public:
		RendererVisBuf() = default;
		~RendererVisBuf() override = default;

		void init_renderer_resources_() override;
		void record_render_commands_() override;

	private:
		void init_programresult_(util::ProgramResult& result) override;

		void init_passes_() override;

		eng::GPUResource vis_buffer_;
		PassVisibility pass_visibility_;
		PassVisBufResolve pass_resolve_;
	};
}
