#pragma once

#include <d3d12.h>

#include "util/Constants.h"
#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"

namespace eng {
    class GPUResource;
    class ResourceManagerSampler;
    class ResourceManagerShader;
}

namespace rndr {

    struct PassDonutDeferredLightingResources {
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::ResourceManagerSampler* sampler_manager = nullptr;

        eng::GPUResource* constant_buffers[util::Constants::FRAME_COUNT]{};

        eng::GPUResource* buf_shadow_map = nullptr;
        eng::GPUResource* buf_diffuse_light_probe = nullptr;
        eng::GPUResource* buf_specular_light_probe = nullptr;
        eng::GPUResource* buf_env_brdf = nullptr;
        eng::GPUResource* depth = nullptr;
        eng::GPUResource* gbuffers[4]{};
        eng::GPUResource* buf_ibl_diffuse = nullptr;
        eng::GPUResource* buf_ibl_specular = nullptr;
        eng::GPUResource* buf_shadow = nullptr;
        eng::GPUResource* buf_ao = nullptr;

        eng::GPUResource* uav_output{};
    };

    class PassDonutDeferredLighting {

    public:
        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            const PassDonutDeferredLightingResources& resources);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            UINT width,
            UINT height);

    private:
        enum class RootParam : UINT {
            CONSTANT_BUFFER,
            SM_LIGHT_ENVBRDF,
            SAMPLER,
            DEPTH_GBUFFER,
            IBL_SHADOW_AO,
            OUTPUT,
        };

        PassDonutDeferredLightingResources resources_{};
        eng::GraphicsPipeline pso_;
    };

}
