#include "dx_util/DX12Context.h"

#include "util/Utils.h"

namespace dxutl {

    DX12Context DX12Context::create_context(
        HWND hwnd, UINT width, UINT height,
        DXGI_FORMAT format, UINT buffer_count) {
        DX12Context context{};

        // debug layer

        UINT factory_flags = 0;

#if defined(_DEBUG)
        Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller;

        if (SUCCEEDED(D3D12GetDebugInterface(
            IID_PPV_ARGS(debug_controller.ReleaseAndGetAddressOf())))) {
            debug_controller->EnableDebugLayer();
            factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
#endif

        // create factory

        Microsoft::WRL::ComPtr<IDXGIFactory4> factory;

        Utils::throw_if_failed(
            CreateDXGIFactory2(
                factory_flags,
                IID_PPV_ARGS(factory.ReleaseAndGetAddressOf())),
            "create DXGI factory"
        );

        // create device

        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

        for (UINT index = 0;; ++index) {
            const HRESULT hr = factory->EnumAdapters1(
                index,
                adapter.ReleaseAndGetAddressOf()
            );

            if (hr == DXGI_ERROR_NOT_FOUND) {
                break;
            }

            Utils::throw_if_failed(hr, "enumerate adapter");

            DXGI_ADAPTER_DESC1 adapter_desc{};

            Utils::throw_if_failed(
                adapter->GetDesc1(&adapter_desc),
                "get adapter description"
            );

            if (adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }

            const HRESULT create_device_hr = D3D12CreateDevice(
                adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(context.device.ReleaseAndGetAddressOf())
            );

            if (SUCCEEDED(create_device_hr)) {
                break;
            }
        }

        if (!context.device) {
            Microsoft::WRL::ComPtr<IDXGIAdapter> warp_adapter;

            Utils::throw_if_failed(
                factory->EnumWarpAdapter(
                    IID_PPV_ARGS(warp_adapter.ReleaseAndGetAddressOf())),
                "enumerate WARP adapter"
            );

            Utils::throw_if_failed(
                D3D12CreateDevice(
                    warp_adapter.Get(),
                    D3D_FEATURE_LEVEL_11_0,
                    IID_PPV_ARGS(context.device.ReleaseAndGetAddressOf())),
                "create WARP device"
            );
        }

        // create queue

        D3D12_COMMAND_QUEUE_DESC queue_desc{};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queue_desc.NodeMask = 0;

        Utils::throw_if_failed(
            context.device->CreateCommandQueue(
                &queue_desc,
                IID_PPV_ARGS(context.queue.ReleaseAndGetAddressOf())),
            "create command queue"
        );

        // tearing support

        BOOL tearing_supported = FALSE;

        Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;

        if (SUCCEEDED(factory.As(&factory5))) {
            if (FAILED(factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &tearing_supported,
                sizeof(tearing_supported)))) {
                tearing_supported = FALSE;
            }
        }

        // create swapchain

        DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
        swap_chain_desc.Width = width;
        swap_chain_desc.Height = height;
        swap_chain_desc.Format = format;
        swap_chain_desc.Stereo = FALSE;
        swap_chain_desc.SampleDesc.Count = 1;
        swap_chain_desc.SampleDesc.Quality = 0;
        swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.BufferCount = buffer_count;
        swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
        swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swap_chain_desc.Flags = tearing_supported
            ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
            : 0;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain1;

        Utils::throw_if_failed(
            factory->CreateSwapChainForHwnd(
                context.queue.Get(),
                hwnd,
                &swap_chain_desc,
                nullptr,
                nullptr,
                swap_chain1.ReleaseAndGetAddressOf()),
            "create swap chain"
        );

        Utils::throw_if_failed(
            factory->MakeWindowAssociation(
                hwnd,
                DXGI_MWA_NO_ALT_ENTER),
            "make window association"
        );

        Utils::throw_if_failed(
            swap_chain1.As(&context.swap_chain),
            "query IDXGISwapChain4"
        );

        context.frame_index = context.swap_chain->GetCurrentBackBufferIndex();

        return context;
    }

}