#pragma once

#include "ProgramArgument.h"
#include "dx_util/DxGraphicsPSO.h"
#include "scene/SceneResources.h"

namespace rndr {

    struct PassVisibilityResources {
        ID3D12Resource* visibility = nullptr;
        ID3D12Resource* depth = nullptr;
        ID3D12Resource* constant_buffers[2]{};
        scene::SceneResources scene{};
    };

    class PassVisibility {
    public:
        void init(ID3D12Device* device, const util::ProgramArgument& arguments,
            const PassVisibilityResources& resources);
        void render(ID3D12GraphicsCommandList* command_list, UINT frame_index,
            const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor_rect);

    private:
        PassVisibilityResources resources_{};
        dxutl::DxGraphicsPSO pso_;
    };

}
