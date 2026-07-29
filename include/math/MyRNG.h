#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <random>
#include <stdexcept>
#include <numeric>

namespace math {

    class MyRNG {
    public:
        using Seed = std::uint32_t;

        explicit MyRNG(Seed seed)
            : engine_(seed) {}

        std::mt19937& get() {
            return engine_;
        }

        const std::mt19937& get() const {
            return engine_;
        }

        void seed(Seed seed) {
            engine_.seed(seed);
        }

        template <typename RandomIt>
        void shuffle(RandomIt first, RandomIt last) {
            std::shuffle(first, last, engine_);
        }

        template <typename Container>
        void shuffle(Container& values) {
            this->shuffle(std::begin(values), std::end(values));
        }

        template <typename Container>
        decltype(auto) sample_from(Container& values) {
            if (values.empty()) {
                throw std::out_of_range(
                    "Cannot sample from an empty container.");
            }

            std::uniform_int_distribution<std::size_t> dist(
                0, values.size() - 1);

            return values[dist(engine_)];
        }

        template <typename Container>
        decltype(auto) sample_from(const Container& values) {
            if (values.empty()) {
                throw std::out_of_range(
                    "Cannot sample from an empty container.");
            }

            std::uniform_int_distribution<std::size_t> dist(
                0, values.size() - 1);

            return values[dist(engine_)];
        }

        Seed sample() {
            return engine_();
        }

        Seed sample(Seed min, Seed max) {
            std::uniform_int_distribution<Seed> dist(min, max);
            return dist(engine_);
        }

        double sample_double() {
            std::uniform_real_distribution<double> dist(0, 1.0);
            return dist(engine_);
        }

        double sample_double(double min, double max) {
            std::uniform_real_distribution<double> dist(min, max);
            return dist(engine_);
        }

        template<typename T>
        std::vector<T> generate_permutation(T bin_count) {
            std::vector<T> permutation(bin_count);
            std::iota(permutation.begin(), permutation.end(), T{ 0 });
            this->shuffle(permutation);
            return permutation;
        }

    private:
        std::mt19937 engine_;
    };

} // namespace math