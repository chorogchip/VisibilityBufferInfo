#pragma once

#include "util/Constants.h"
#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include "scene/donut/DonutSceneDataGPU.h"

namespace eng {
    class GPUResource;
    class ResourceManagerFrame;
    class ResourceManagerShader;
}

namespace rndr {

    struct PassDonutDepthPreResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* depth = nullptr;
        eng::GPUResource* constant_buffers[util::Constants::FRAME_COUNT]{};
        const scene::DonutSceneDataGPU* scene = nullptr;
    };

    class PassDonutDepthPre {

    public:
        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            const PassDonutDepthPreResources& resources,
            bool use_prepass_depth);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            const D3D12_VIEWPORT& viewport,
            const D3D12_RECT& scissor_rect);

    private:
        enum class RootParam : UINT {
            PUSH_CONSTANT,
            VIEW_CONSTANT,
            INSTANCE_BUFFER,
            VERTEX_BUFFER,
        };

        struct PushConstants {
            uint32_t start_instance_location = 0;
            uint32_t start_vertex_location = 0;
            uint32_t position_offset = 0;
            uint32_t texcoord_offset = 0;
        };

        static constexpr UINT PUSH_CONSTANT_DWORD_COUNT =
            sizeof(PushConstants) / sizeof(uint32_t);

        PassDonutDepthPreResources resources_{};
        eng::GraphicsPipeline pso_;
    };
}
