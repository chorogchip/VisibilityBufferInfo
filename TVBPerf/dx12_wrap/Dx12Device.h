#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace dx12 {
    class Dx12Device {
    public:
        static Dx12Device create();

        [[nodiscard]] ID3D12Device* get() const noexcept { return device_.Get(); }
        [[nodiscard]] IDXGIFactory4* get_factory() const noexcept { return factory_.Get(); }
        [[nodiscard]] IDXGIAdapter1* get_adapter() const noexcept { return adapter_.Get(); }

    private:
        Microsoft::WRL::ComPtr<IDXGIFactory6> factory_;
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_;
        Microsoft::WRL::ComPtr<ID3D12Device> device_;
    };
}