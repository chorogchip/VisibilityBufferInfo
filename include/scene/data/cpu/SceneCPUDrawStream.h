#pragma once

#include <cstdint>
#include <vector>

#include "scene/data/cpu/SceneCPUData.h"

namespace scene {

    struct SceneCPUDrawStream {
        std::vector<uint32_t> draw_instance_ids_compacted;
        std::vector<SceneCPUData::DrawCall> draw_calls_compacted;
    };
}
