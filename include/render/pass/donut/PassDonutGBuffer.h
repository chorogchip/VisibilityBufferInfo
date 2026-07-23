#pragma once

#include "util/Constants.h"
#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include "scene/donut/DonutSceneDataGPU.h"

namespace eng {
    class GPUResource;
    class ResourceManagerFrame;
    class ResourceManagerSampler;
    class ResourceManagerShader;
}

namespace rndr {

    struct PassDonutGBufferResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerSampler* sampler_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* depth = nullptr;
        eng::GPUResource* gbuffers[4]{};
        eng::GPUResource* constant_buffers[util::Constants::FRAME_COUNT]{};
        const scene::DonutSceneDataGPU* scene = nullptr;
    };

    class PassDonutGBuffer {

    public:
        static constexpr UINT MATERIAL_TEXTURE_DESCRIPTOR_COUNT =
            scene::DonutSceneDataGPU::MATERIAL_TEXTURE_DESCRIPTOR_COUNT;

        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            const PassDonutGBufferResources& resources,
            bool use_prepass_depth);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            const D3D12_VIEWPORT& viewport,
            const D3D12_RECT& scissor_rect);

    private:

        PassDonutGBufferResources resources_{};
        eng::GraphicsPipeline pso_;
        bool use_prepass_depth_ = false;
        bool use_motion_vectors_ = false;
    };
}
