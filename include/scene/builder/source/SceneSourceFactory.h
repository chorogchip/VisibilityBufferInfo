#pragma once

#include <memory>

#include "ProgramArgument.h"
#include "scene/data/source/SceneSourceData.h"

namespace scene {

    class SceneSourceFactory {
    public:
        static bool uses_jungle_builder(
            const util::ProgramArgument& argument);
        static std::unique_ptr<SceneSourceData> create_scene(const util::ProgramArgument& argument);
    };
}
