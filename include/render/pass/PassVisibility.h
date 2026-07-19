#pragma once

#include "Constants.h"
#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include "scene/SceneDataCPU.h"

namespace eng { class ResourceManagerFrame; class ResourceManagerShader; }

namespace rndr {

    struct PassVisibilityResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        ID3D12Resource* visibility = nullptr;
        ID3D12Resource* depth = nullptr;
        ID3D12Resource* constant_buffers[util::FRAME_COUNT]{};
        ID3D12Resource* instance_buffer = nullptr;
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
        D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
        const scene::SceneDataCPU* scene = nullptr;
    };

    class PassVisibility {
    public:
        void init(ID3D12Device* device, const util::ProgramArgument& arguments,
            const PassVisibilityResources& resources);
        void render(ID3D12GraphicsCommandList* command_list, UINT frame_index,
            const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor_rect);

    private:
        PassVisibilityResources resources_{};
        eng::GraphicsPipeline pso_;
    };

}
