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
		virtual void make_programresult(util::ProgramResult& result) override;
		void render_() override;

		D3D12_RESOURCE_STATES depth_stencil_initial_state() const override;
        void init_passes() override;

		bool do_prepass_ = false;
		PassDepthPre pass_depth_pre_;
		PassForward pass_forward_;
	};
}
