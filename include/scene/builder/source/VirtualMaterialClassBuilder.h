#pragma once

#include <cstdint>

#include "scene/data/source/SceneSourceData.h"

namespace scene {

    class VirtualMaterialClassBuilder {

    public:
        enum class EnumAssignStrategy : uint8_t {
            RANDOM = 0,
            PBR_FEATURE = 1,
        };

        struct Params {
            EnumAssignStrategy strategy = EnumAssignStrategy::RANDOM;
            uint32_t max_material_bin_limit = 255;
            uint32_t max_material_real_open = 1;
            uint32_t seed = 0;
            float material_diversity = 1.0f;  // 0: linearly biased, 1: uniform(same amount)
        };

        static void assign_materials(
            SceneSourceData& scene,
            const Params& params);
    };
}