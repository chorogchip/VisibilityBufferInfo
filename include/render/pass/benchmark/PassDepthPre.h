#pragma once

#include "util/Constants.h"
#include "engine/GraphicsPipeline.h"
#include "ProgramArgument.h"
#include "scene/SceneDataCPU.h"

namespace eng {
    class GPUResource;
    class ResourceManagerFrame;
    class ResourceManagerShader;
}

namespace rndr {

    struct PassDepthPreResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* depth = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS constant_buffer_addresses[util::Constants::FRAME_COUNT]{};
        D3D12_GPU_VIRTUAL_ADDRESS instance_buffer_address = 0;
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
        D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
        const scene::SceneDataCPU* scene = nullptr;
    };

    class PassDepthPre {

    public:
        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            const PassDepthPreResources& resources);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            const D3D12_VIEWPORT& viewport,
            const D3D12_RECT& scissor_rect);

    private:
        PassDepthPreResources resources_{};
        eng::GraphicsPipeline pso_;
    };

}
