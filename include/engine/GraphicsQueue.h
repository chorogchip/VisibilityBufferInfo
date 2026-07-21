#pragma once

#include <d3d12.h>
#include <wrl.h>

namespace eng {

    class GraphicsQueue {
    public:
        GraphicsQueue() = default;
        ~GraphicsQueue();

        GraphicsQueue(const GraphicsQueue&) = delete;
        GraphicsQueue& operator=(const GraphicsQueue&) = delete;
        GraphicsQueue(GraphicsQueue&&) = delete;
        GraphicsQueue& operator=(GraphicsQueue&&) = delete;

        void init(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

        void execute(ID3D12CommandList* command_list);

        [[nodiscard]] UINT64 signal();
        void wait(UINT64 fence_value);
        void wait(const GraphicsQueue& queue, UINT64 fence_value);
        void wait_idle();

        [[nodiscard]] ID3D12CommandQueue* get() const {
            return queue_.Get();
        }

    private:
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
        Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
        HANDLE fence_event_ = nullptr;
        UINT64 next_fence_value_ = 1;
    };

}
