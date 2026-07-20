#include "render/pass/donut/PassDonutDepthPre.h"

namespace rndr {

    void PassDonutDepthPre::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutDepahPreResources& resources,
        bool use_prepass_depth) {


    }

    void PassDonutDepthPre::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {


    }
}