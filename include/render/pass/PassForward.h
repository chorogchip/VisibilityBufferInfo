#pragma once

#include <d3d12.h>

#include "ProgramArgument.h"
#include "dx_util/DxGraphicsPSO.h"
#include "scene/SceneResources.h"

namespace rndr {

    struct PassForwardResources {
        ID3D12Resource* back_buffers[2]{};
        ID3D12Resource* depth = nullptr;
        ID3D12Resource* constant_buffers[2]{};
        scene::SceneResources scene{};
        ID3D12DescriptorHeap* sampler_heap = nullptr;
    };

    class PassForward {
    public:
        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            const PassForwardResources& resources,
            bool use_prepass_depth = false);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            const D3D12_VIEWPORT& viewport,
            const D3D12_RECT& scissor_rect);

    private:
        PassForwardResources resources_{};
        dxutl::DxGraphicsPSO pso_;
        bool use_prepass_depth_ = false;
    };

}
