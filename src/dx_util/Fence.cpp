#include "dx_util/Fence.h"

#include <d3d12.h>
#include "util/Utils.h"

namespace dxutl {

    Fence::~Fence() {
        if (fence_event_) {
            CloseHandle(fence_event_);
            fence_event_ = nullptr;
        }
    }

    void Fence::init(ID3D12Device* p_device, ID3D12CommandQueue* p_queue) {
        if (fence_ || fence_event_ || queue_) throw std::logic_error("Fence is already initialized");

        fence_value_ = 0;
        queue_ = p_queue;
        Utils::throw_if_failed(p_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "fence create");

        fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!fence_event_) Utils::throw_win32_lasterr("fence event create");
    }

    UINT64 Fence::signal() {
        ++fence_value_;
        Utils::throw_if_failed(queue_->Signal(fence_.Get(), fence_value_), "fence signal");
        return fence_value_;
    }

    void Fence::wait_for_value(UINT64 value) {
        if (fence_->GetCompletedValue() < value) {
            Utils::throw_if_failed(fence_->SetEventOnCompletion(value, fence_event_), "fence set event");
            DWORD result = WaitForSingleObjectEx(fence_event_, INFINITE, FALSE);
            if (result != WAIT_OBJECT_0) Utils::throw_win32_lasterr("fence wait");
        }
    }

    void Fence::wait_for_gpu() {
        UINT64 val = this->signal();
        this->wait_for_value(val);
    }
}
