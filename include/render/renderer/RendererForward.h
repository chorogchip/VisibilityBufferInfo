#pragma once

#include "render/renderer/RendererBase.h"
#include "render/pass/PassDepthPre.h"
#include "render/pass/PassForward.h"

namespace rndr {

	class RendererForward : public RendererBase {
	public:
		explicit RendererForward(bool do_prepass);
		~RendererForward() override = default;

	private:
		virtual void init_programresult_(util::ProgramResult& result) override;
		void record_render_commands_() override;

        void init_passes_() override;

		bool do_prepass_ = false;
		PassDepthPre pass_depth_pre_;
		PassForward pass_forward_;
	};
}
