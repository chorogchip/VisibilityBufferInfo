#pragma once

#include "scene/data/cpu/SceneCPUData.h"
#include "scene/data/source/SceneSourceData.h"

namespace scene {

    class SceneCPUBuilder {
    public:
        static SceneCPUData build(const SceneSourceData& source);
    };
}
