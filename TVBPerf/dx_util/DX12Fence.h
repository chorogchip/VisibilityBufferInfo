#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <cstdint>

namespace dxutl {

	class DX12Fence {

	public:
		DX12Fence() = default;
		~DX12Fence();

		UINT64 signal();
		void wait_for_value(UINT64 value);
		void wait_for_gpu();

		operator bool() const { return fence_.Get() != nullptr && fence_event_; }

		static DX12Fence create_fence(
			ID3D12Device* p_device,
			ID3D12CommandQueue* p_queue);

	private:
		Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
		UINT64 fence_value_ = 0;
		HANDLE fence_event_ = nullptr;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
	};
}