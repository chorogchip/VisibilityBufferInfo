#pragma once

#include <memory>

#include "ProgramArgument.h"
#include "scene/SceneDataCPU.h"

namespace scene {

    class SceneLoader {
    public:
        static std::unique_ptr<SceneDataCPU> load(const util::ProgramArgument& arg);
    };

}
