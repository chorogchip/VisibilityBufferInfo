#pragma once

#include <memory>

#include "ProgramArgument.h"
#include "scene/data/source/SceneSourceData.h"

namespace scene {

    class SceneSourceFactory {
    public:
        static std::unique_ptr<SceneSourceData> create_scene(const util::ProgramArgument& argument);
    };
}
