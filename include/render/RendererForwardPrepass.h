#pragma once

#include "render/RendererBase.h"

namespace rndr {

	class RendererForwardPrePass : public RendererBase {
	public:
		RendererForwardPrePass() = default;
		~RendererForwardPrePass() override = default;

		void render_() override;

	private:
		virtual void make_programresult(util::ProgramResult& result) override;

		UINT dsv_descriptor_count() const override;
		D3D12_RESOURCE_STATES depth_stencil_initial_state() const override;
		void create_extra_depth_stencil_views() override;
		UINT srv_descriptor_count() const override;
		void create_shader_resources() override;
		void create_root_signature() override;
		void create_pso() override;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_depth_prepass_;
	};
}
