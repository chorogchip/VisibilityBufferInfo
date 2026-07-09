#pragma once

#include <memory>

#include "util/ProgramArgument.h"
#include "scene/SceneDataCPU.h"

namespace scene {

    class SceneLoader {
    public:
        static std::unique_ptr<SceneDataCPU> load(const ProgramArgument& arg);
    };

}
