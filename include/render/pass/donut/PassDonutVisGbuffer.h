#pragma once

#include "util/Constants.h"
#include "ProgramArgument.h"
#include "engine/GPUResource.h"
#include "engine/GraphicsPipeline.h"
#include "scene/donut/DonutSceneDataGPU.h"

namespace eng {
    class ResourceManagerFrame;
    class ResourceManagerSampler;
    class ResourceManagerShader;
}

namespace rndr {

    struct PassDonutVisGBufferResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerSampler* sampler_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* visibility = nullptr;
        eng::GPUResource* depth = nullptr;
        eng::GPUResource* gbuffers[4]{};
        eng::GPUResource* constant_buffers[util::Constants::FRAME_COUNT]{};
    };

    class PassDonutVisGBuffer {

    public:
        static constexpr UINT MATERIAL_TEXTURE_DESCRIPTOR_COUNT =
            scene::DonutSceneDataGPU::MATERIAL_TEXTURE_DESCRIPTOR_COUNT;

        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            const PassDonutVisGBufferResources& resources);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            const D3D12_VIEWPORT& viewport,
            const D3D12_RECT& scissor_rect);

    private:

        PassDonutVisGBufferResources resources_{};
        eng::GraphicsPipeline pso_;
        bool use_motion_vectors_ = false;
    };
}
