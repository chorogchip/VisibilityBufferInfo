#pragma once

#include "scene/data/cpu/SceneCPUData.h"
#include "scene/data/source/SceneSourceData.h"

namespace scene {

    // Converts renderer-independent source data into draw-ready CPU data.
    class SceneCPUBuilder {
    public:
        static SceneCPUData build(const SceneSourceData& source);
    };
}
