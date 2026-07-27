#pragma once

#include "util/Constants.h"
#include "ProgramArgument.h"
#include "engine/GPUResource.h"
#include "engine/GraphicsPipeline.h"
#include "scene/data/gpu/DonutSceneGPUData.h"

namespace eng {
    class ResourceManagerShader;
}

namespace rndr {

    struct PassDonutVisUtilResources {
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* visibility_buf = nullptr;
        eng::GPUResource* objects = nullptr;
        eng::GPUResource* materials = nullptr;
        eng::GPUResource* pixel_list = nullptr;
    };

    class PassDonutVisUtil {

    public:
        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            const PassDonutVisUtilResources& resources);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT width, UINT height);

    private:
        static constexpr inline uint32_t MAX_SHADER_COUNT = 256;

        PassDonutVisUtilResources resources_{};
        eng::GraphicsPipeline pso_binning_;
        eng::GraphicsPipeline pso_prefixscan_;
        eng::GraphicsPipeline pso_flatten_;

        eng::GPUResource bin_counts_{};
        eng::GPUResource bin_prefix_{};
    };
}
