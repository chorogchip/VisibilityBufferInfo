#include "GraphicsUtils.h"

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#include "Utils.h"

using Microsoft::WRL::ComPtr;

void GraphicsUtils::compile_shader(ID3DBlob** pp_blob, LPCWSTR filename, LPCSTR shader_model, D3D_SHADER_MACRO* defines) {
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

    UINT compileFlags = 0;

#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(
        filename, defines, nullptr, "main", shader_model,
        compileFlags, 0, pp_blob, &error_blob
    );

    if (FAILED(hr))
    {
        if (error_blob)
        {
            OutputDebugStringA(static_cast<const char*>(error_blob->GetBufferPointer()));
        }
        Utils::throw_if_failed(hr, "compile shader");
    }
}

void GraphicsUtils::create_buffer(ComPtr<ID3D12Resource>& buffer, ID3D12Device* p_device, UINT64 width, UINT height,
    D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_STATES state,
    D3D12_RESOURCE_FLAGS flags, DXGI_FORMAT format, D3D12_CLEAR_VALUE* clear_value) {

    D3D12_HEAP_PROPERTIES heap_props{};
    heap_props.Type = heap_type;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1;
    heap_props.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = height == 1 ? D3D12_RESOURCE_DIMENSION_BUFFER : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_desc.Alignment = 0;
    resource_desc.Width = width;
    resource_desc.Height = height;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = format;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = height == 1 ? D3D12_TEXTURE_LAYOUT_ROW_MAJOR : D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_desc.Flags = flags;

    Utils::throw_if_failed(p_device->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,
        state,
        clear_value,
        IID_PPV_ARGS(&buffer)), "create buffer");
}

void GraphicsUtils::copy_cpu_to_upload(ID3D12Resource* dest, const void* src, size_t sz) {

    void* mapped_data = nullptr;

    D3D12_RANGE readRange = {};
    readRange.Begin = 0;
    readRange.End = 0;

    Utils::throw_if_failed(dest->Map(0, &readRange, &mapped_data), "map instance buffer");
    memcpy(mapped_data, src, sz);
    dest->Unmap(0, nullptr);
}

void* GraphicsUtils::get_mapped_address(ID3D12Resource* dest) {
    void* mapped_data = nullptr;

    D3D12_RANGE readRange = {};
    readRange.Begin = 0;
    readRange.End = 0;

    Utils::throw_if_failed(dest->Map(0, &readRange, &mapped_data), "map instance buffer");
    return mapped_data;
}

void GraphicsUtils::record_transition(ID3D12GraphicsCommandList* p_list, ID3D12Resource* p_resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after) {

    D3D12_RESOURCE_BARRIER barrier[1]{};
    barrier[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier[0].Transition.pResource = p_resource;
    barrier[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier[0].Transition.StateBefore = state_before;
    barrier[0].Transition.StateAfter = state_after;

    p_list->ResourceBarrier(1, barrier);
}