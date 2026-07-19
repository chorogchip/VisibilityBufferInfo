#include "engine/GPUResource.h"

#include "util/Logger.h"

namespace eng {

    void GPUResource::attach(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES initial_state) {
        util::Logger::g_logger.assert_with_log(
            resource != nullptr, "GPU resource attach requires a resource");
        resource_ = resource;
        state_ = initial_state;
    }

    void GPUResource::reset() {
        resource_.Reset();
        state_ = D3D12_RESOURCE_STATE_COMMON;
    }

    void GPUResource::transition(
        ID3D12GraphicsCommandList* command_list,
        D3D12_RESOURCE_STATES after) {
        util::Logger::g_logger.assert_with_log(
            command_list != nullptr && resource_ != nullptr,
            "GPU resource transition requires a command list and resource");

        if (state_ == after)
            return;

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource_.Get();
        barrier.Transition.StateBefore = state_;
        barrier.Transition.StateAfter = after;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        command_list->ResourceBarrier(1, &barrier);
        state_ = after;
    }

    void GPUResource::uav_barrier(
        ID3D12GraphicsCommandList* command_list) const {
        util::Logger::g_logger.assert_with_log(
            command_list != nullptr && resource_ != nullptr,
            "GPU UAV barrier requires a command list and resource");

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = resource_.Get();
        command_list->ResourceBarrier(1, &barrier);
    }

}
