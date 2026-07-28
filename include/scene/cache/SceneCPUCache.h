#pragma once

#include <memory>

#include "ProgramArgument.h"
#include "scene/data/cpu/SceneCPUData.h"

namespace scene {

    std::unique_ptr<SceneCPUData> load_or_build_scene_cpu(
        const util::ProgramArgument& argument);

}
