#pragma once

#include <cstdint>

namespace util {

    class Constants {

    public:
        static inline constexpr std::uint32_t FRAME_COUNT = 2;
        static inline constexpr std::uint32_t MAX_PASS_COUNT = 31;
        static inline constexpr std::uint32_t TIMER_SLOT_COUNT = MAX_PASS_COUNT + 1;
    };
}
