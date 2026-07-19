#pragma once

#include "ProgramArgument.h"
#include "dx_util/DxGraphicsPSO.h"

namespace rndr {

    struct PassDeferredLightingResources {
        ID3D12Resource* back_buffers[2]{};
        ID3D12Resource* gbuffers[8]{};
        UINT gbuffer_count = 0;
    };

    class PassDeferredLighting {
    public:
        void init(ID3D12Device* device, const util::ProgramArgument& arguments,
            const PassDeferredLightingResources& resources);
        void render(ID3D12GraphicsCommandList* command_list, UINT frame_index,
            const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor_rect);

    private:
        PassDeferredLightingResources resources_{};
        dxutl::DxGraphicsPSO pso_;
    };

}
