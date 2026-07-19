#pragma once

#include <vector>

#include "render/renderer/RendererBase.h"
#include "render/pass/PassDeferredLighting.h"
#include "render/pass/PassDepthPre.h"
#include "render/pass/PassGBuffer.h"

namespace rndr {

	class RendererDeferred : public RendererBase {
	public:
		explicit RendererDeferred(bool do_prepass);
		~RendererDeferred() override = default;

	private:
		void render_() override;
		void make_programresult(util::ProgramResult& result) override;
		void create_renderer_resources() override;

		D3D12_RESOURCE_STATES depth_stencil_initial_state() const override;
		void init_passes() override;

		bool do_prepass_ = false;
		PassDepthPre pass_depth_pre_;
		PassGBuffer pass_gbuffer_;
		PassDeferredLighting pass_lighting_;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> gbuffers_;
	};
}
