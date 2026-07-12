#pragma once

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace dxutl {

    Microsoft::WRL::ComPtr<ID3D12Device> create_device(
        Microsoft::WRL::ComPtr<IDXGIFactory4>& factory);

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> create_command_queue(
        ID3D12Device* device);

    Microsoft::WRL::ComPtr<IDXGISwapChain3> create_swapchain(
        IDXGIFactory4* factory,
        ID3D12CommandQueue* command_queue,
        HWND hwnd,
        UINT width,
        UINT height,
        UINT frame_count);

}
