#pragma once

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace dxutl {

    Microsoft::WRL::ComPtr<ID3D12Resource> create_committed_resource(
        ID3D12Device* device,
        const D3D12_RESOURCE_DESC& resource_desc,
        D3D12_HEAP_TYPE heap_type,
        D3D12_RESOURCE_STATES initial_state,
        const D3D12_CLEAR_VALUE* clear_value = nullptr);

    Microsoft::WRL::ComPtr<ID3D12Resource> create_buffer(
        ID3D12Device* device,
        UINT64 size_in_bytes,
        D3D12_HEAP_TYPE heap_type,
        D3D12_RESOURCE_STATES initial_state);

    Microsoft::WRL::ComPtr<ID3D12Resource> create_texture2d(
        ID3D12Device* device,
        UINT64 width,
        UINT height,
        DXGI_FORMAT format,
        D3D12_HEAP_TYPE heap_type,
        D3D12_RESOURCE_STATES initial_state,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        const D3D12_CLEAR_VALUE* clear_value = nullptr);

    Microsoft::WRL::ComPtr<ID3D12Resource> create_texture2d_array(
        ID3D12Device* device,
        UINT64 width,
        UINT height,
        UINT16 array_size,
        DXGI_FORMAT format,
        D3D12_RESOURCE_STATES initial_state,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        const D3D12_CLEAR_VALUE* clear_value = nullptr);

    Microsoft::WRL::ComPtr<ID3D12Resource> create_depth_stencil_buffer(
        ID3D12Device* device,
        UINT64 width,
        UINT height,
        DXGI_FORMAT format,
        D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_DEPTH_WRITE,
        float clear_depth = 1.0f,
        UINT8 clear_stencil = 0);

    void create_depth_stencil_view(
        ID3D12Device* device,
        ID3D12Resource* depth_stencil_buffer,
        DXGI_FORMAT format,
        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle,
        D3D12_DSV_FLAGS flags = D3D12_DSV_FLAG_NONE);

    void copy_to_upload_buffer(
        ID3D12Resource* upload_buffer,
        const void* source,
        size_t size_in_bytes);

    void* map_upload_buffer(
        ID3D12Resource* upload_buffer);

    void transition_resource(
        ID3D12GraphicsCommandList* command_list,
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after,
        UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

}
