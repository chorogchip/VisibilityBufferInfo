#pragma once

#include <d3d12.h>

#include "util/Constants.h"
#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include "scene/SceneDataCPU.h"
#include "engine/MaterialGPU.h"
#include <vector>

namespace eng {
    class GPUResource;
    class ResourceManagerFrame;
    class ResourceManagerSampler;
    class ResourceManagerShader;
}

namespace rndr {

    struct PassForwardResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* back_buffers[util::Constants::FRAME_COUNT]{};
        eng::GPUResource* depth = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS constant_buffer_addresses[util::Constants::FRAME_COUNT]{};
        D3D12_GPU_VIRTUAL_ADDRESS instance_buffer_address = 0;
        D3D12_GPU_VIRTUAL_ADDRESS material_buffer_address = 0;
        std::vector<eng::GPUResource*> material_textures;
        eng::ResourceManagerSampler* sampler_manager = nullptr;
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
        D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
        const scene::SceneDataCPU* scene = nullptr;
        const std::vector<eng::MaterialGPU>* materials;
        bool to_use_textures = false;
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
        eng::GraphicsPipeline pso_;
        bool use_prepass_depth_ = false;
    };

}
