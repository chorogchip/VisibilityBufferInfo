#pragma once

#include "util/Constants.h"
#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include <vector>
#include "scene/SceneDataCPU.h"

namespace eng {
    class GPUResource;
    class ResourceManagerFrame;
    class ResourceManagerSampler;
    class ResourceManagerShader;
}

namespace rndr {
    struct PassVisBufGBufferResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* visibility = nullptr;
        eng::GPUResource* vertex_buffer = nullptr;
        eng::GPUResource* index_buffer = nullptr;
        eng::GPUResource* mesh_buffer = nullptr;
        eng::GPUResource* instance_buffer = nullptr;
        eng::GPUResource* material_buffer = nullptr;
        std::vector<eng::GPUResource*> material_textures;
        const scene::SceneDataCPU* scene = nullptr;
        eng::GPUResource* gbuffers[8]{};
        UINT gbuffer_count = 0;
        D3D12_GPU_VIRTUAL_ADDRESS constant_buffer_addresses[util::Constants::FRAME_COUNT]{};
        eng::ResourceManagerSampler* sampler_manager = nullptr;
    };

    class PassVisBufGBuffer {

    public:
        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            const PassVisBufGBufferResources& resources);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            const D3D12_VIEWPORT& viewport,
            const D3D12_RECT& scissor_rect);

    private:
        PassVisBufGBufferResources resources_{};
        eng::GraphicsPipeline pso_;
    };
}
