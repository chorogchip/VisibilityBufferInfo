#include "DX12List.h"

#include "util/Utils.h"

namespace dxutl {

    void DX12List::reset(ID3D12CommandAllocator* allocator, ID3D12PipelineState* pso) {
        Utils::throw_if_failed(command_list->Reset(allocator, pso), "reset list");

    }

    void DX12List::close() {
        Utils::throw_if_failed(command_list->Close(), "close list");
    }


    void DX12List::execute(ID3D12CommandQueue* queue) {
        ID3D12CommandList* command_lists[] = { command_list.Get() };
        queue->ExecuteCommandLists(_countof(command_lists), command_lists);
    }

    DX12List DX12List::create_list(
        ID3D12Device* device,
        ID3D12CommandAllocator* allocator) {

        DX12List ret{};

        Utils::throw_if_failed(device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator,
            nullptr,
            IID_PPV_ARGS(ret.command_list.ReleaseAndGetAddressOf())),
            "create command list");

        Utils::throw_if_failed(ret.command_list->Close(),
            "command list close");

        return ret;
    }
}