#include "dx_util/DeviceUtils.h"

#include "util/Utils.h"

namespace dxutl {

    Microsoft::WRL::ComPtr<ID3D12Device> create_device(
        Microsoft::WRL::ComPtr<IDXGIFactory4>& factory) {
#if defined(_DEBUG)
        {
            Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debug_controller.ReleaseAndGetAddressOf())))) {
                debug_controller->EnableDebugLayer();
            }
        }
#endif

        Utils::throw_if_failed(
            CreateDXGIFactory1(IID_PPV_ARGS(factory.ReleaseAndGetAddressOf())),
            "create DXGI factory");

        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        Microsoft::WRL::ComPtr<ID3D12Device> device;
        for (UINT i = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(i, &adapter); ++i) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            if (SUCCEEDED(D3D12CreateDevice(
                adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(device.ReleaseAndGetAddressOf())))) {
                return device;
            }
        }

        Microsoft::WRL::ComPtr<IDXGIAdapter> warp_adapter;
        Utils::throw_if_failed(
            factory->EnumWarpAdapter(IID_PPV_ARGS(warp_adapter.ReleaseAndGetAddressOf())),
            "enumerate adapter");
        Utils::throw_if_failed(D3D12CreateDevice(
            warp_adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(device.ReleaseAndGetAddressOf())),
            "create device");
        return device;
    }

    Microsoft::WRL::ComPtr<IDXGISwapChain3> create_swapchain(
        IDXGIFactory4* factory,
        ID3D12CommandQueue* command_queue,
        HWND hwnd,
        UINT width,
        UINT height,
        UINT frame_count) {

        DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
        swap_chain_desc.BufferCount = frame_count;
        swap_chain_desc.Width = width;
        swap_chain_desc.Height = height;
        swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swap_chain_desc.SampleDesc.Count = 1;
        swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
        Utils::throw_if_failed(factory->CreateSwapChainForHwnd(
            command_queue,
            hwnd,
            &swap_chain_desc,
            nullptr,
            nullptr,
            &swap_chain), "create swapchain");

        Utils::throw_if_failed(factory->MakeWindowAssociation(
            hwnd,
            DXGI_MWA_NO_ALT_ENTER), "factory make window association");

        Microsoft::WRL::ComPtr<IDXGISwapChain3> swapchain;
        Utils::throw_if_failed(swap_chain.As(&swapchain), "swapchain as");
        return swapchain;
    }

}
