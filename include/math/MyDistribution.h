#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "math/MyRNG.h"

namespace math {

    class MyDistribution {

    public:
        static MyDistribution generate_diversed(
            uint32_t bin_count, uint32_t total_sum, float diversity);

        uint32_t sample(MyRNG& rng) const;
        uint32_t sample_normalized(double norm) const;

    private:
        std::vector<uint32_t> data_;
    };

}