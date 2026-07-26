#pragma once

#include "scene/data/source/SceneSourceData.h"

namespace scene {

    class SceneSourceHierarchyValidator {
    public:
        static void validate(const source::Node& node);
        static void validate(const SceneSourceData& scene);
    };
}
