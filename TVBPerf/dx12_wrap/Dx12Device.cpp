#include "Dx12Device.h"

#include <utility>

#include "util/Utils.h"

namespace dx12 {

    Dx12Device Dx12Device::create() {
        Dx12Device result{};

        UINT factory_flags = 0;

#if defined(_DEBUG)
        Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller;

        const HRESULT debug_hr = D3D12GetDebugInterface(
            IID_PPV_ARGS(debug_controller.ReleaseAndGetAddressOf())
        );

        if (SUCCEEDED(debug_hr)) {
            debug_controller->EnableDebugLayer();
            factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
#endif

        Utils::throw_if_failed(
            CreateDXGIFactory2(
                factory_flags,
                IID_PPV_ARGS(result.factory_.ReleaseAndGetAddressOf())
            ),
            "create DXGI factory"
        );

        HRESULT last_device_error = DXGI_ERROR_UNSUPPORTED;

        for (UINT index = 0;; ++index) {
            Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

            const HRESULT enum_hr =
                result.factory_->EnumAdapterByGpuPreference(
                    index,
                    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())
                );

            if (enum_hr == DXGI_ERROR_NOT_FOUND) {
                break;
            }

            Utils::throw_if_failed(enum_hr, "enumerate GPU adapter");

            DXGI_ADAPTER_DESC1 adapter_desc{};

            Utils::throw_if_failed(
                adapter->GetDesc1(&adapter_desc),
                "get GPU adapter description"
            );

            if ((adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
                continue;
            }

            Microsoft::WRL::ComPtr<ID3D12Device> device;

            last_device_error = D3D12CreateDevice(
                adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(device.ReleaseAndGetAddressOf())
            );

            if (SUCCEEDED(last_device_error)) {
                result.adapter_ = std::move(adapter);
                result.device_ = std::move(device);
                break;
            }
        }

        if (!result.device_) {
            Utils::throw_if_failed(
                result.factory_->EnumWarpAdapter(
                    IID_PPV_ARGS(result.adapter_.ReleaseAndGetAddressOf())
                ),
                "enumerate WARP adapter"
            );

            Utils::throw_if_failed(
                D3D12CreateDevice(
                    result.adapter_.Get(),
                    D3D_FEATURE_LEVEL_11_0,
                    IID_PPV_ARGS(result.device_.ReleaseAndGetAddressOf())
                ),
                "create WARP device"
            );
        }

#if defined(_DEBUG)
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> info_queue;

        if (SUCCEEDED(result.device_.As(&info_queue))) {
            info_queue->SetBreakOnSeverity(
                D3D12_MESSAGE_SEVERITY_CORRUPTION,
                TRUE
            );
            info_queue->SetBreakOnSeverity(
                D3D12_MESSAGE_SEVERITY_ERROR,
                TRUE
            );
        }
#endif

        return result;
    }

} // namespace dx12