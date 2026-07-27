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
        const scene::DonutSceneGPUData* scene = nullptr;
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

        [[nodiscard]] const eng::GPUResource& bin_counts() const {
            return bin_counts_;
        }
        [[nodiscard]] const eng::GPUResource& bin_prefix() const {
            return bin_prefix_;
        }

    private:
        static constexpr inline uint32_t MAX_SHADER_COUNT = 256;
        static constexpr inline uint32_t MAX_REAL_SHADER_ID =
            MAX_SHADER_COUNT - 2;

        PassDonutVisUtilResources resources_{};
        eng::GraphicsPipeline pso_clear_counts_;
        eng::GraphicsPipeline pso_binning_;
        eng::GraphicsPipeline pso_prefixscan_;
        eng::GraphicsPipeline pso_flatten_;

        eng::GPUResource bin_counts_{};
        eng::GPUResource bin_prefix_{};
    };
}
