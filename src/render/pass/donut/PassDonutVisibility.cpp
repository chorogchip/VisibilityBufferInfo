#include "render/pass/donut/PassDonutVisibility.h"


namespace eng {
    enum class RootParams {

    };
}

namespace rndr {

    void PassDonutVisibility::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutVisibilityResources& resources,
        bool use_prepass_depth) {

    }

    void PassDonutVisibility::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {

    }
}
