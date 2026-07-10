#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace dxutl {
	struct DX12List {
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list;


		void reset(ID3D12CommandAllocator* allocator, ID3D12PipelineState* pso);
		void close();
		void execute(ID3D12CommandQueue* queue);

		static DX12List create_list(
			ID3D12Device* device,
			ID3D12CommandAllocator* allocator);
	};
}