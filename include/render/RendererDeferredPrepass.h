#pragma once

#include <vector>

#include "render/RendererBase.h"

namespace rndr {

	class RendererDeferredPrepass : public RendererBase {
	public:
		RendererDeferredPrepass() = default;
		~RendererDeferredPrepass() override = default;

		void configure_pass() override;
		void create_pass_resources() override;
		void render_() override;

	private:
		UINT rtv_descriptor_count() const override;
		void create_extra_render_target_views(D3D12_CPU_DESCRIPTOR_HANDLE next_rtv_handle) override;
		UINT srv_descriptor_count() const override;
		void create_shader_resources() override;
		void create_root_signature() override;
		void create_pso() override;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_lighting_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_depth_prepass_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_lighting_;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> gbuffers_;
		uint32_t gbuffer_count_ = 0;
	};
}
