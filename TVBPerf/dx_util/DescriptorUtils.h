#pragma once

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace dxutl {

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> create_descriptor_heap(
        ID3D12Device* device,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        UINT descriptor_count,
        D3D12_DESCRIPTOR_HEAP_FLAGS flags,
        const char* error_message = "create descriptor heap");

    UINT descriptor_size(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type);

    D3D12_CPU_DESCRIPTOR_HANDLE offset_cpu_descriptor(
        D3D12_CPU_DESCRIPTOR_HANDLE handle,
        UINT descriptor_size,
        UINT offset);

    D3D12_GPU_DESCRIPTOR_HANDLE offset_gpu_descriptor(
        D3D12_GPU_DESCRIPTOR_HANDLE handle,
        UINT descriptor_size,
        UINT offset);

    void create_swapchain_render_target_views(
        ID3D12Device* device,
        IDXGISwapChain3* swapchain,
        ID3D12DescriptorHeap* rtv_heap,
        UINT rtv_descriptor_size,
        UINT frame_count,
        Microsoft::WRL::ComPtr<ID3D12Resource>* render_targets);
}
