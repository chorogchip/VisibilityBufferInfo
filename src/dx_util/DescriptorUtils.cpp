#include "dx_util/DescriptorUtils.h"

#include <dxgi1_6.h>

#include "util/Utils.h"

namespace dxutl {

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> create_descriptor_heap(
        ID3D12Device* device,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        UINT descriptor_count,
        D3D12_DESCRIPTOR_HEAP_FLAGS flags,
        const char* error_message) {

        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.NumDescriptors = descriptor_count;
        heap_desc.Type = type;
        heap_desc.Flags = flags;

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
        Utils::throw_if_failed(
            device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf())),
            error_message);
        return heap;
    }

    UINT descriptor_size(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type) {
        return device->GetDescriptorHandleIncrementSize(type);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE offset_cpu_descriptor(
        D3D12_CPU_DESCRIPTOR_HANDLE handle,
        UINT descriptor_size,
        UINT offset) {
        handle.ptr += static_cast<SIZE_T>(offset) * descriptor_size;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE offset_gpu_descriptor(
        D3D12_GPU_DESCRIPTOR_HANDLE handle,
        UINT descriptor_size,
        UINT offset) {
        handle.ptr += static_cast<UINT64>(offset) * descriptor_size;
        return handle;
    }

    void create_swapchain_render_target_views(
        ID3D12Device* device,
        IDXGISwapChain3* swapchain,
        ID3D12DescriptorHeap* rtv_heap,
        UINT rtv_descriptor_size,
        UINT frame_count,
        Microsoft::WRL::ComPtr<ID3D12Resource>* render_targets) {

        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < frame_count; ++i) {
            Utils::throw_if_failed(
                swapchain->GetBuffer(i, IID_PPV_ARGS(render_targets[i].ReleaseAndGetAddressOf())),
                "create rtv");
            device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtv_handle);
            rtv_handle.ptr += rtv_descriptor_size;
        }
    }

    static D3D12_INPUT_ELEMENT_DESC input_layout[] =
    {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        {
            "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
            24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        {
            "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0,
            32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        {
            "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        }
    };

    D3D12_INPUT_LAYOUT_DESC get_default_input_layout_desc() {

        D3D12_INPUT_LAYOUT_DESC desc{};
        desc.pInputElementDescs = input_layout;
        desc.NumElements = _countof(input_layout);
        return desc;
    }
}
