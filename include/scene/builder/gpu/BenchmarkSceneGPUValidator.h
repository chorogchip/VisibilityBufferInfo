#pragma once

#include "scene/data/gpu/BenchmarkSceneGPUData.h"

namespace scene {

    class BenchmarkSceneGPUValidator {
    public:
        static void validate(const BenchmarkSceneGPUData& scene);
    };
}
