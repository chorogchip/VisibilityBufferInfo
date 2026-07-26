#pragma once

#include "ProgramArgument.h"
#include "scene/data/source/SceneSourceData.h"

namespace scene {

    class SceneSourceLoader {
    public:
        static SceneSourceData load(
            const util::ProgramArgument& argument);
    };
}
