#pragma once

#include <d3d12.h>

#include "Constants.h"
#include "ProgramArgument.h"

namespace eng { class ResourceManagerSampler; class ResourceManagerShader; }

namespace rndr {

    struct PassDonutDeferredLightingResources {
        eng::ResourceManagerShader* shader_manager = nullptr;
        ID3D12Resource* depth = nullptr;
        ID3D12Resource* gbuffers[4]{};
        ID3D12Resource* output = nullptr;
        ID3D12Resource* constant_buffers[util::FRAME_COUNT]{};
        eng::ResourceManagerSampler* sampler_manager = nullptr;
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
        PassDonutDeferredLightingResources resources_{};
    };

}
