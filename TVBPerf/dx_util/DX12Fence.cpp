#include "DX12Fence.h"

#include <d3d12.h>
#include "util/Utils.h"

namespace dxutl {

    DX12Fence::~DX12Fence() {
        if (fence_event_) {
            CloseHandle(fence_event_);
            fence_event_ = nullptr;
        }
    }

    UINT64 DX12Fence::signal() {
        ++fence_value_;
        Utils::throw_if_failed(queue_->Signal(fence_.Get(), fence_value_), "fence signal");
        return fence_value_;
    }

    void DX12Fence::wait_for_value(UINT64 value) {
        if (fence_->GetCompletedValue() < value) {
            Utils::throw_if_failed(fence_->SetEventOnCompletion(value, fence_event_), "fence set event");
            DWORD result = WaitForSingleObjectEx(fence_event_, INFINITE, FALSE);
            if (result != WAIT_OBJECT_0) Utils::throw_win32_lasterr("fence wait");
        }
    }

    void DX12Fence::wait_for_gpu() {
        UINT64 val = this->signal();
        this->wait_for_value(val);
    }

    DX12Fence DX12Fence::create_fence(
        ID3D12Device* device,
        ID3D12CommandQueue* queue) {

        DX12Fence ret{};

        ret.queue_ = queue;
        Utils::throw_if_failed(device->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(ret.fence_.ReleaseAndGetAddressOf())),
            "fence create");

        ret.fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!ret.fence_event_) Utils::throw_win32_lasterr("fence event create");
    }
}