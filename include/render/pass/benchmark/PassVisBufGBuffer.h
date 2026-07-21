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
        ID3D12Resource* vertex_buffer = nullptr;
        ID3D12Resource* index_buffer = nullptr;
        ID3D12Resource* mesh_buffer = nullptr;
        ID3D12Resource* instance_buffer = nullptr;
        ID3D12Resource* material_buffer = nullptr;
        std::vector<ID3D12Resource*> material_textures;
        const scene::SceneDataCPU* scene = nullptr;
        eng::GPUResource* gbuffers[8]{};
        UINT gbuffer_count = 0;
        ID3D12Resource* constant_buffers[util::Constants::FRAME_COUNT]{};
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
