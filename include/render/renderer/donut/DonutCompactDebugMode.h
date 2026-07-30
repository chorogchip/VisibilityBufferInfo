#pragma once

#include <cstdint>

namespace rndr {

    enum class DonutCompactDebugMode : std::uint32_t {
        GlobalCompactIndex = 0,
        BinLocalDispatchThread,
        BinLocalGroup,
        GroupThread,
    };

}
