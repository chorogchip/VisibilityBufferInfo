#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace dxutl {
	struct DX12Context {

		Microsoft::WRL::ComPtr<ID3D12Device> device;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
		Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain;
		UINT frame_index = 0;

		static DX12Context create_context(
			HWND hwnd, UINT width, UINT height,
			DXGI_FORMAT format, UINT buffer_count);
	};
}