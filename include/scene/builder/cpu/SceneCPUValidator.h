#pragma once

#include "scene/data/cpu/SceneCPUData.h"

namespace scene {

    class SceneCPUValidator {
    public:
        static void validate(const SceneCPUData& scene);
    };
}
