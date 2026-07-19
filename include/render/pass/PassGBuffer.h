#pragma once

#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include "scene/SceneDataCPU.h"
#include <vector>

namespace eng { class ResourceManagerFrame; class ResourceManagerShader; }

namespace rndr {

    struct PassGBufferResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        ID3D12Resource* gbuffers[8]{};
        UINT gbuffer_count = 0;
        ID3D12Resource* depth = nullptr;
        ID3D12Resource* constant_buffers[2]{};
        ID3D12Resource* instance_buffer = nullptr;
        ID3D12Resource* material_buffer = nullptr;
        std::vector<ID3D12Resource*> material_textures;
        ID3D12DescriptorHeap* sampler_heap = nullptr;
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
        D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
        const scene::SceneDataCPU* scene = nullptr;
    };

    class PassGBuffer {
    public:
        void init(ID3D12Device* device, const util::ProgramArgument& arguments,
            const PassGBufferResources& resources, bool use_prepass_depth);
        void render(ID3D12GraphicsCommandList* command_list, UINT frame_index,
            const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor_rect);

    private:
        PassGBufferResources resources_{};
        eng::GraphicsPipeline pso_;
        bool use_prepass_depth_ = false;
    };

}
