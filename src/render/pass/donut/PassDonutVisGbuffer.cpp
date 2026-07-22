#include "render/pass/donut/PassDonutVisGbuffer.h"

namespace rndr {

    namespace {

        enum class RootParams {

        };
    }

    void PassDonutGBuffer::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutGBufferResources& resources) {


    }

    void PassDonutGBuffer::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {

    }
}
