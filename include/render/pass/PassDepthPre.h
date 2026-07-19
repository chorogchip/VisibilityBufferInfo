#pragma once

#include "engine/GraphicsPipeline.h"
#include "ProgramArgument.h"
#include "scene/SceneDataCPU.h"

namespace eng { class ResourceManagerFrame; class ResourceManagerShader; }

namespace rndr {

    struct PassDepthPreResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        ID3D12Resource* depth = nullptr;
        ID3D12Resource* constant_buffers[2]{};
        ID3D12Resource* instance_buffer = nullptr;
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
