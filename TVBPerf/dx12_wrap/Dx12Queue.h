#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>
#include <Windows.h>

#include "dx12_wrap/Dx12Device.h"

namespace dx12 {

	class Dx12Queue {
	public:
		Dx12Queue() = default;
		~Dx12Queue();

		Dx12Queue(const Dx12Queue&) = delete;
		Dx12Queue& operator=(const Dx12Queue&) = delete;

		Dx12Queue(Dx12Queue&& other) noexcept;
		Dx12Queue& operator=(Dx12Queue&& other) noexcept;

		[[nodiscard]]
		static Dx12Queue create(
			const Dx12Device& device,
			D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

		void execute(
			ID3D12CommandList* command_list);
		void execute(
			UINT command_list_count,
			ID3D12CommandList* const* command_lists);

		[[nodiscard]] UINT64 signal();
		void wait_for_value(UINT64 value);
		void wait_for_gpu();
		[[nodiscard]] UINT64 completed_value() const;

		[[nodiscard]] ID3D12CommandQueue* get() const noexcept { return queue_.Get() }
		[[nodiscard]] ID3D12Fence* get_fence() const noexcept { return fence_.Get(); }
		[[nodiscard]] D3D12_COMMAND_LIST_TYPE type() const noexcept { return type_; }
		[[nodiscard]]
		explicit operator bool() const noexcept {
			return queue_ != nullptr
				&& fence_ != nullptr
				&& fence_event_ != nullptr;
		}

	private:
		void release() noexcept;
		void move_from(Dx12Queue&& other) noexcept;

	private:
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
		Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

		UINT64 fence_value_ = 0;
		HANDLE fence_event_ = nullptr;

		D3D12_COMMAND_LIST_TYPE type_ = D3D12_COMMAND_LIST_TYPE_DIRECT;
	};

}