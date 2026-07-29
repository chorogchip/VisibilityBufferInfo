#include "math/MyOrdering.h"

#include <algorithm>
#include <cstdint>

namespace math {

    namespace {

        static uint64_t spread_bits_32(uint32_t value) {
            uint64_t x = value;

            x = (x | (x << 16)) & 0x0000FFFF0000FFFFULL;
            x = (x | (x << 8)) & 0x00FF00FF00FF00FFULL;
            x = (x | (x << 4)) & 0x0F0F0F0F0F0F0F0FULL;
            x = (x | (x << 2)) & 0x3333333333333333ULL;
            x = (x | (x << 1)) & 0x5555555555555555ULL;

            return x;
        }
    }

    double MyOrdering::z_order(uint32_t x, uint32_t y, uint32_t width) {

        if (width == 0) return 0.0f;

        x = (std::min)(x, width - 1);
        y = (std::min)(y, width - 1);

        uint32_t wid = width - 1;
        uint32_t bits = 0;
        do { ++bits; } while (wid >>= 1);

        if (bits == 0) return 0.0f;

        const uint64_t morton_code =
            spread_bits_32(x) |
            (spread_bits_32(y) << 1);

        const uint32_t morton_bits = bits * 2;

        double range = static_cast<double>(1ULL << (bits * 2));
        return static_cast<double>(morton_code) / range;
    }

}