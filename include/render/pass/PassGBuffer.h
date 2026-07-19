#pragma once

#include "ProgramArgument.h"
#include "dx_util/DxGraphicsPSO.h"
#include "scene/SceneResources.h"

namespace rndr {

    struct PassGBufferResources {
        ID3D12Resource* gbuffers[8]{};
        UINT gbuffer_count = 0;
        ID3D12Resource* depth = nullptr;
        ID3D12Resource* constant_buffers[2]{};
        scene::SceneResources scene{};
        ID3D12DescriptorHeap* sampler_heap = nullptr;
    };

    class PassGBuffer {
    public:
        void init(ID3D12Device* device, const util::ProgramArgument& arguments,
            const PassGBufferResources& resources, bool use_prepass_depth);
        void render(ID3D12GraphicsCommandList* command_list, UINT frame_index,
            const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor_rect);

    private:
        PassGBufferResources resources_{};
        dxutl::DxGraphicsPSO pso_;
        bool use_prepass_depth_ = false;
    };

}
