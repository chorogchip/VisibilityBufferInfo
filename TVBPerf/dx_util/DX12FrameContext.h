#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace dxutl {
	struct DX12FrameContext {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator;
		Microsoft::WRL::ComPtr<ID3D12Resource> render_target;

		Microsoft::WRL::ComPtr<ID3D12Resource> camera_constant_buffer;
		void* camera_constant_buffer_mapped = nullptr;

		UINT64 fence_value = 0;

		static DX12FrameContext create_frame_context(
			ID3D12Device* device);
	};
}
