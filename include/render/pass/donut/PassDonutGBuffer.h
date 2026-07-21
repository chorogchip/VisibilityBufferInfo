#pragma once

#include "util/Constants.h"
#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include "scene/donut/DonutSceneDataGPU.h"

namespace eng {
    class GPUResource;
    class ResourceManagerFrame;
    class ResourceManagerSampler;
    class ResourceManagerShader;
}

namespace rndr {

    struct PassDonutGBufferResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerSampler* sampler_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* depth = nullptr;
        eng::GPUResource* gbuffers[4]{};
        eng::GPUResource* constant_buffers[util::Constants::FRAME_COUNT]{};
        const scene::DonutSceneDataGPU* scene = nullptr;
    };

    class PassDonutGBuffer {

    public:
        static constexpr UINT MATERIAL_TEXTURE_DESCRIPTOR_COUNT = 7u;

        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            const PassDonutGBufferResources& resources,
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
            GEOMETRY_DATA,
            MATERIAL_CONSTANT,
            MATERIAL_TEXTURES,
            MATERIAL_SAMPLER,
        };

        struct PushConstants {
            uint32_t start_instance_location = 0;
            uint32_t start_vertex_location = 0;
            uint32_t position_offset = 0;
            uint32_t prev_position_offset = 0;
            uint32_t texcoord_offset = 0;
            uint32_t normal_offset = 0;
            uint32_t tangent_offset = 0;
        };

        static constexpr UINT PUSH_CONSTANT_DWORD_COUNT =
            sizeof(PushConstants) / sizeof(uint32_t);
        static constexpr UINT MATERIAL_TEXTURE_SOURCE_SLOT_COUNT =
            static_cast<UINT>(scene::DonutSceneDataCPU::MATERIAL_TEXTURE_SLOT_COUNT);

        PassDonutGBufferResources resources_{};
        eng::GraphicsPipeline pso_;
        bool use_prepass_depth_ = false;
        bool use_motion_vectors_ = false;
    };
}
