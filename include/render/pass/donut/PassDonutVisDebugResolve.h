#pragma once

#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include "render/renderer/VisibilityDebugMode.h"
#include "scene/data/gpu/DonutSceneGPUData.h"
#include "util/Constants.h"

namespace eng {
    class GPUResource;
    class ResourceManagerFrame;
    class ResourceManagerShader;
}

namespace rndr {

    struct PassDonutVisDebugResolveResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* back_buffers[util::Constants::FRAME_COUNT]{};
        eng::GPUResource* visibility = nullptr;
        eng::GPUResource* constant_buffers[util::Constants::FRAME_COUNT]{};
        const scene::DonutSceneGPUData* scene = nullptr;
    };

    class PassDonutVisDebugResolve {
    public:
        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            VisibilityDebugMode mode,
            const PassDonutVisDebugResolveResources& resources);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            const D3D12_VIEWPORT& viewport,
            const D3D12_RECT& scissor_rect);

    private:
        PassDonutVisDebugResolveResources resources_{};
        eng::GraphicsPipeline pso_;
    };

}
