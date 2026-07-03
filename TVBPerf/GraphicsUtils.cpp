#include "GraphicsUtils.h"

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#include "Utils.h"

using Microsoft::WRL::ComPtr;

void GraphicsUtils::compile_shader(ID3DBlob** pp_blob, LPCWSTR filename, LPCSTR shader_model) {
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

    UINT compileFlags = 0;

#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(
        filename, nullptr, nullptr, "main", shader_model,
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

void GraphicsUtils::create_device(ComPtr<IDXGIFactory4>& factory, ComPtr<ID3D12Device>& device) {
#if defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debug_controller;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
        {
            debug_controller->EnableDebugLayer();
        }
    }
#endif

Utils::throw_if_failed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "create DXGI factory");

ComPtr<IDXGIAdapter1> adapter;

for (UINT i = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(i, &adapter); ++i)
{
    DXGI_ADAPTER_DESC1 desc;
    adapter->GetDesc1(&desc);

    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        continue;

    if (SUCCEEDED(D3D12CreateDevice(
        adapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device))))
    {
        return;
    }
}

ComPtr<IDXGIAdapter> warp_adapter;
Utils::throw_if_failed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)), "enumerate adapter");

Utils::throw_if_failed(D3D12CreateDevice(
    warp_adapter.Get(),
    D3D_FEATURE_LEVEL_11_0,
    IID_PPV_ARGS(&device)), "create device");
}

void GraphicsUtils::create_command_objects(ID3D12Device* p_device, ComPtr<ID3D12CommandQueue>& command_queue, ComPtr<ID3D12GraphicsCommandList>& command_list, ComPtr<ID3D12CommandAllocator> allocator_lists[], int allocator_count) {

    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    Utils::throw_if_failed(p_device->CreateCommandQueue(
        &queue_desc,
        IID_PPV_ARGS(&command_queue)), "create command queue");

    for (int i = 0; i < allocator_count; ++i) {
        Utils::throw_if_failed(p_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&allocator_lists[i])), "create command allocator");
    }

    Utils::throw_if_failed(p_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator_lists[0].Get(),
        nullptr,
        IID_PPV_ARGS(&command_list)), "create command list");

    Utils::throw_if_failed(command_list->Close(), "command list close");
}

void GraphicsUtils::create_swapchain(HWND hwnd, IDXGIFactory4* p_factory, ID3D12Device* p_device, ID3D12CommandQueue* p_command_queue, ComPtr<IDXGISwapChain3>& swapchain, UINT width, UINT height, UINT frame_count) {

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
    swap_chain_desc.BufferCount = frame_count;
    swap_chain_desc.Width = width;
    swap_chain_desc.Height = height;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGISwapChain1> swap_chain;

    Utils::throw_if_failed(p_factory->CreateSwapChainForHwnd(
        p_command_queue,
        hwnd,
        &swap_chain_desc,
        nullptr,
        nullptr,
        &swap_chain), "create swapchain");

    Utils::throw_if_failed(p_factory->MakeWindowAssociation(
        hwnd,
        DXGI_MWA_NO_ALT_ENTER), "factory make window association");

    Utils::throw_if_failed(swap_chain.As(&swapchain), "swapchain as");
}

void GraphicsUtils::create_buffer(ComPtr<ID3D12Resource>& buffer, ID3D12Device* p_device, UINT64 width, UINT64 height, D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_STATES state) {

    D3D12_HEAP_PROPERTIES heap_props{};
    heap_props.Type = heap_type;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1;
    heap_props.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Alignment = 0;
    resource_desc.Width = width;
    resource_desc.Height = height;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    Utils::throw_if_failed(p_device->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,
        state,
        nullptr,
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