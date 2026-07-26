#pragma once

#include <cstdint>
#include <limits>

namespace scene::source {

    struct SceneConstants {
        static constexpr uint32_t INVALID_INDEX =
            (std::numeric_limits<uint32_t>::max)();
    };
}
