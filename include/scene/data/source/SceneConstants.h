#pragma once

#include <cstdint>
#include <limits>

#include "util/minmax_remover.h"

namespace scene::source {

    struct SceneConstants {
        static constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();
    };
}
