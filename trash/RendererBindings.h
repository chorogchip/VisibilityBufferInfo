#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>

namespace dxutl {
	class RendererBindings {

	public:
		RendererBindings() = default;
		~RendererBindings() = default;

		void build(
			ID3D12Device* device);

		void add_descriptor(
			ID3D12Resource* resource,
			DXGI_FORMAT format,
			D3D12_SRV_DIMENSION dimention,
			UINT heap_index);

	private:
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srv_heap_;
		UINT src_heap_max_size_ = 0;
		UINT srv_descriptor_size_ = 0;
		struct SRV_DESC_INFO {
			D3D12_SHADER_RESOURCE_VIEW_DESC src_desc;
			ID3D12Resource* resource;
			UINT index;
		};
		std::vector<SRV_DESC_INFO> srv_descs_;


		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
		std::vector<D3D12_ROOT_PARAMETER> root_parameters_;
	};
}