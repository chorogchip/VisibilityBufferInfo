#pragma once

#include "Constants.h"
#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include "scene/SceneDataCPU.h"
#include <vector>

namespace eng {
    class GPUResource;
    class ResourceManagerFrame;
    class ResourceManagerSampler;
    class ResourceManagerShader;
}

namespace rndr {

    // TODO make resources for GBuffer pass

    struct PassDonutGBufferResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerSampler* sampler_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* depth = nullptr;  // PS
        eng::GPUResource* gbuffers[4]{};  // PS
        eng::GPUResource* material_constant_buffers[util::FRAME_COUNT]{};  // PS
        eng::GPUResource* gbuf_constant_buffers[util::FRAME_COUNT]{};  // shared
        eng::GPUResource* instance_buffer = nullptr;  // VS
        eng::GPUResource* vertex_buffer = nullptr;  // VS
        const scene::SceneDataCPU* scene = nullptr;
    };

    class PassDonutGBuffer {

    public:

        struct GBufferPushConstants {
            uint32_t start_instance_location;
            uint32_t start_vetex_location;
            uint32_t position_offset;
            uint32_t prev_position_offset;
            uint32_t texcoord_offset;
            uint32_t normal_offset;
            uint32_t tangent_offset;
        };

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
        PassDonutGBufferResources resources_{};
        eng::GraphicsPipeline pso_;
        bool use_prepass_depth_ = false;
        bool use_motion_vectors_ = false;

        GBufferPushConstants push_constants_;
    };
}
