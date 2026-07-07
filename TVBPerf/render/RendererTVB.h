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
		void create_dsv_heap() override;
		void create_rtv_heap() override;
		void create_render_targets() override;
		void create_depth_stencil_buffer() override;
		void create_srv_heap() override;
		void create_shader_resources() override;
		void create_root_signature() override;
		void create_pso() override;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_lighting_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_lighting_;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srv_heap_;
		UINT srv_descriptor_size_ = 0;
		Microsoft::WRL::ComPtr<ID3D12Resource> mesh_buffer_;
		Microsoft::WRL::ComPtr<ID3D12Resource> vis_buffer_;
	};
}