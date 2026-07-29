#include "math/MyDistribution.h"

#include <algorithm>
#include <cmath>

namespace math {

    MyDistribution MyDistribution::generate_diversed(
        uint32_t bin_count, uint32_t total_sum, float diversity) {

        MyDistribution ret{};
        ret.data_.reserve(bin_count);

        const double t = std::clamp(static_cast<double>(diversity), 0.0, 1.0);

        for (size_t i = 0; i + 1 < bin_count; ++i) {
            
            const uint32_t prefix_biased = total_sum;
            const uint64_t prefix_diversed =
                static_cast<uint64_t>(total_sum) * (i + 1) / bin_count;

            const double prefix_interpolated = std::floor(
                (1.0 - t) * static_cast<double>(prefix_biased) +
                t * static_cast<double>(prefix_diversed));

            ret.data_.push_back(static_cast<uint32_t>(prefix_interpolated));
        }
        ret.data_.push_back(total_sum);
        return ret;
    }

    uint32_t MyDistribution::sample(MyRNG& rng) const {
        const uint32_t total_sum = data_.back();
        const uint32_t target = rng.sample(0, total_sum - 1);
        const auto it = std::upper_bound(data_.begin(), data_.end(), target);
        return static_cast<uint32_t>(std::distance(data_.begin(), it));
    }
    uint32_t MyDistribution::sample_normalized(double norm) const {
        const uint32_t total_sum = data_.back();
        const uint32_t target = static_cast<uint32_t>(
            norm * static_cast<double>(total_sum));
        const auto it = std::upper_bound(data_.begin(), data_.end(), target);
        return static_cast<uint32_t>(std::distance(data_.begin(), it));
    }
}