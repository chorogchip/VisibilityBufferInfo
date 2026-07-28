#pragma once

#include "scene/data/source/SceneSourceData.h"

namespace scene {

    class SceneSourceSemanticValidator {
    public:
        static void validate(const SceneSourceData& scene);
    };

} // namespace scene
