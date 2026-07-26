#pragma once

#include "scene/data/source/SceneSourceData.h"

namespace scene {

    class SceneSourceDataValidator {
    public:
        static void validate(const SceneSourceData& scene);
    };
}
