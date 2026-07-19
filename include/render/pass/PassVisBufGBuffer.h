#pragma once

#include "ProgramArgument.h"
#include "dx_util/DxGraphicsPSO.h"
#include "render/VisBufResources.h"

namespace rndr {
    struct PassVisBufGBufferResources {
        VisBufResources visbuf{};
        ID3D12Resource* gbuffers[8]{};
        UINT gbuffer_count = 0;
        ID3D12Resource* constant_buffers[2]{};
        ID3D12DescriptorHeap* sampler_heap = nullptr;
    };

    class PassVisBufGBuffer {
    public:
        void init(ID3D12Device*, const util::ProgramArgument&, const PassVisBufGBufferResources&);
        void render(ID3D12GraphicsCommandList*, UINT, const D3D12_VIEWPORT&, const D3D12_RECT&);
    private:
        PassVisBufGBufferResources resources_{};
        dxutl::DxGraphicsPSO pso_;
    };
}
