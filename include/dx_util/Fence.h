#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <cstdint>

namespace dxutl {

	class Fence {

	public:
		Fence() = default;
		~Fence();

		Fence(const Fence&) = delete;
		Fence& operator=(const Fence&) = delete;

		void init(
			ID3D12Device* p_device,
			ID3D12CommandQueue* p_queue);

		UINT64 signal();
		void wait_for_value(UINT64 value);
		void wait_for_gpu();
		operator bool() const { return fence_.Get() != nullptr && fence_event_; }

	private:
		Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
		UINT64 fence_value_ = 0;
		HANDLE fence_event_ = nullptr;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
	};
}