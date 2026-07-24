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
}

namespace rndr {

    struct PassDebugViewResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::GPUResource* back_buffers[util::Constants::FRAME_COUNT]{};
        eng::GPUResource* depth = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS constant_buffer_addresses[util::Constants::FRAME_COUNT]{};
        D3D12_GPU_VIRTUAL_ADDRESS instance_buffer_address = 0;
        D3D12_GPU_VIRTUAL_ADDRESS material_buffer_address = 0;
        std::vector<eng::GPUResource*> material_textures;
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
        D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
        const scene::SceneDataCPU* scene = nullptr;
        const std::vector<eng::MaterialGPU>* materials;
    };

    class PassDebugView {

    public:
        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            const PassDebugViewResources& resources);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            const D3D12_VIEWPORT& viewport,
            const D3D12_RECT& scissor_rect);

    private:
        PassDebugViewResources resources_{};
        eng::GraphicsPipeline pso_;
    };
}
