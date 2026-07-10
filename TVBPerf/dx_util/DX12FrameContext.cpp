#include "DX12FrameContext.h"

#include "util/Utils.h"

namespace dxutl {
    DX12FrameContext DX12FrameContext::create_frame_context(
        ID3D12Device* device) {

        DX12FrameContext ret{};

        Utils::throw_if_failed(device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(ret.command_allocator.ReleaseAndGetAddressOf())),
            "create command allocator");
        
        ret.fence_value = 1;

        return ret;
    }
}