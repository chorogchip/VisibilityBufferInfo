#include "engine/GraphicsQueue.h"

#include "util/Logger.h"

namespace eng {

    GraphicsQueue::~GraphicsQueue() {
        if (fence_event_) {
            CloseHandle(fence_event_);
            fence_event_ = nullptr;
        }
    }

    void GraphicsQueue::init(ID3D12Device* device) {
        D3D12_COMMAND_QUEUE_DESC queue_desc{};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

        HRESULT result = device->CreateCommandQueue(
            &queue_desc,
            IID_PPV_ARGS(queue_.ReleaseAndGetAddressOf()));
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "create graphics queue");

        result = device->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(fence_.ReleaseAndGetAddressOf()));
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "create graphics queue fence");

        fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        util::Logger::g_logger.assert_with_log(fence_event_ != nullptr, "create graphics queue fence event");
        next_fence_value_ = 1;
    }

    void GraphicsQueue::execute(ID3D12CommandList* command_list) {
        ID3D12CommandList* command_lists[] = { command_list };
        queue_->ExecuteCommandLists(1, command_lists);
    }

    UINT64 GraphicsQueue::signal() {
        const UINT64 fence_value = next_fence_value_++;
        const HRESULT result = queue_->Signal(fence_.Get(), fence_value);
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "signal graphics queue fence");
        return fence_value;
    }

    void GraphicsQueue::wait(UINT64 fence_value) {
        if (fence_->GetCompletedValue() >= fence_value)
            return;

        HRESULT result = fence_->SetEventOnCompletion(fence_value, fence_event_);
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "set graphics queue fence event");

        const DWORD wait_result = WaitForSingleObjectEx(fence_event_, INFINITE, FALSE);
        util::Logger::g_logger.assert_with_log(wait_result == WAIT_OBJECT_0, "wait for graphics queue fence");
    }

    void GraphicsQueue::wait_idle() {
        wait(signal());
    }

}
