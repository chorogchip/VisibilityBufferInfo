#pragma once

#include "Constants.h"
#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"

namespace eng { class GPUResource; class ResourceManagerFrame; class ResourceManagerShader; }

namespace rndr {

    struct PassDeferredLightingResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* back_buffers[util::FRAME_COUNT]{};
        eng::GPUResource* gbuffers[8]{};
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
        eng::GraphicsPipeline pso_;
    };

}
