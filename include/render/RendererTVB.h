#pragma once

#include <vector>

#include "render/RendererBase.h"

namespace rndr {

	class RendererTVB : public RendererBase {
	public:
		RendererTVB() = default;
		~RendererTVB() override = default;

		void init_() override;
		void render_() override;

	private:
		UINT rtv_descriptor_count() const override;
		void create_extra_render_target_views(D3D12_CPU_DESCRIPTOR_HANDLE next_rtv_handle) override;
		UINT srv_descriptor_count() const override;
		void create_shader_resources() override;
		void create_root_signature() override;
		void create_pso() override;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_lighting_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_lighting_;
		Microsoft::WRL::ComPtr<ID3D12Resource> mesh_buffer_;
		Microsoft::WRL::ComPtr<ID3D12Resource> vis_buffer_;
	};
}
