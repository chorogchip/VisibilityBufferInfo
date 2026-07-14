#pragma once

#include <vector>

namespace util {
    std::vector<unsigned char> create_dummy_texture_data(
        unsigned int width,
        unsigned int height,
        unsigned int seed);
}