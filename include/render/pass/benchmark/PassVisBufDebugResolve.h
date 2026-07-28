#pragma once

#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include "render/renderer/VisibilityDebugMode.h"
#include "scene/data/cpu/SceneCPUData.h"
#include "scene/data/gpu/BenchmarkSceneGPUData.h"
#include "util/Constants.h"

namespace eng {
    class GPUResource;
    class ResourceManagerFrame;
    class ResourceManagerShader;
}

namespace rndr {

    struct PassVisBufDebugResolveResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* back_buffers[util::Constants::FRAME_COUNT]{};
        eng::GPUResource* visibility = nullptr;
        eng::GPUResource* vertex_buffer = nullptr;
        eng::GPUResource* index_buffer = nullptr;
        eng::GPUResource* submesh_buffer = nullptr;
        eng::GPUResource* instance_buffer = nullptr;
        eng::GPUResource* draw_instance_buffer = nullptr;
        const scene::SceneCPUData* scene = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS
            constant_buffer_addresses[util::Constants::FRAME_COUNT]{};
    };

    class PassVisBufDebugResolve {
    public:
        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            VisibilityDebugMode mode,
            const PassVisBufDebugResolveResources& resources);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            const D3D12_VIEWPORT& viewport,
            const D3D12_RECT& scissor_rect);

    private:
        PassVisBufDebugResolveResources resources_{};
        eng::GraphicsPipeline pso_;
    };

}
