#include "engine/GPUQueue.h"

#include "util/Logger.h"

namespace eng {

    GPUQueue::~GPUQueue() {
        if (fence_event_) {
            CloseHandle(fence_event_);
            fence_event_ = nullptr;
        }
    }

    void GPUQueue::init(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type) {
        D3D12_COMMAND_QUEUE_DESC queue_desc{};
        queue_desc.Type = type;
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

        HRESULT result = device->CreateCommandQueue(
            &queue_desc,
            IID_PPV_ARGS(queue_.ReleaseAndGetAddressOf()));
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "create command queue");

        result = device->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(fence_.ReleaseAndGetAddressOf()));
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "create command queue fence");

        fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        util::Logger::g_logger.assert_with_log(fence_event_ != nullptr, "create command queue fence event");
        next_fence_value_ = 1;
    }

    void GPUQueue::execute(ID3D12CommandList* command_list) {
        util::Logger::g_logger.assert_with_log(
            queue_ != nullptr && command_list != nullptr,
            "command queue execute requires an initialized queue and command list");

        ID3D12CommandList* command_lists[] = { command_list };
        queue_->ExecuteCommandLists(1, command_lists);
    }

    UINT64 GPUQueue::signal() {
        util::Logger::g_logger.assert_with_log(
            queue_ != nullptr && fence_ != nullptr,
            "command queue signal requires an initialized queue and fence");

        const UINT64 fence_value = next_fence_value_++;
        const HRESULT result = queue_->Signal(fence_.Get(), fence_value);
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "signal command queue fence");
        return fence_value;
    }

    void GPUQueue::wait(UINT64 fence_value) {
        util::Logger::g_logger.assert_with_log(
            fence_ != nullptr && fence_event_ != nullptr,
            "command queue CPU wait requires an initialized fence");

        if (fence_->GetCompletedValue() >= fence_value)
            return;

        HRESULT result = fence_->SetEventOnCompletion(fence_value, fence_event_);
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "set command queue fence event");

        const DWORD wait_result = WaitForSingleObjectEx(fence_event_, INFINITE, FALSE);
        util::Logger::g_logger.assert_with_log(wait_result == WAIT_OBJECT_0, "wait for command queue fence");
    }

    void GPUQueue::wait(const GPUQueue& queue, UINT64 fence_value) {
        util::Logger::g_logger.assert_with_log(
            queue_ != nullptr && queue.fence_ != nullptr,
            "command queue GPU wait requires initialized queues");

        const HRESULT result = queue_->Wait(queue.fence_.Get(), fence_value);
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "wait for another command queue fence");
    }

    void GPUQueue::wait_idle() {
        if (!queue_)
            return;

        wait(signal());
    }

}
