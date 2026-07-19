#pragma once

#include "ProgramArgument.h"
#include "dx_util/DxGraphicsPSO.h"
#include "render/VisBufResources.h"

namespace rndr {
    struct PassVisBufResolveResources {
        ID3D12Resource* back_buffers[2]{};
        VisBufResources visbuf{};
        ID3D12Resource* constant_buffers[2]{};
        ID3D12DescriptorHeap* sampler_heap = nullptr;
    };

    class PassVisBufResolve {
    public:
        void init(ID3D12Device*, const util::ProgramArgument&, const PassVisBufResolveResources&);
        void render(ID3D12GraphicsCommandList*, UINT, const D3D12_VIEWPORT&, const D3D12_RECT&);
    private:
        PassVisBufResolveResources resources_{};
        dxutl::DxGraphicsPSO pso_;
    };
}
