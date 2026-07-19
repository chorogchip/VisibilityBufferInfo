#pragma once

#include <d3d12.h>
#include <wrl.h>

namespace eng {

    class GPUResource {
    public:
        GPUResource() = default;
        GPUResource(const GPUResource&) = delete;
        GPUResource& operator=(const GPUResource&) = delete;
        GPUResource(GPUResource&&) noexcept = default;
        GPUResource& operator=(GPUResource&&) noexcept = default;

        void attach(
            ID3D12Resource* resource,
            D3D12_RESOURCE_STATES initial_state);

        void reset();

        void transition(
            ID3D12GraphicsCommandList* command_list,
            D3D12_RESOURCE_STATES after);

        void uav_barrier(ID3D12GraphicsCommandList* command_list) const;

        [[nodiscard]] ID3D12Resource* get() const { return resource_.Get(); }
        [[nodiscard]] D3D12_RESOURCE_STATES state() const { return state_; }
        [[nodiscard]] explicit operator bool() const { return resource_ != nullptr; }

    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
        D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COMMON;
    };

}
