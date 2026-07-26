#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

namespace jungle::validation {

    struct Totals {
        uint64_t instances = 0;
        size_t instance_sets = 0;
        size_t cells = 0;
        size_t systems = 0;
        size_t meshes = 0;
        size_t primitives = 0;
        size_t materials = 0;
        size_t images = 0;
        size_t cameras = 0;
        size_t uv1_primitives = 0;
        size_t color0_primitives = 0;
        size_t color1_primitives = 0;
        size_t alpha_blend_materials = 0;
        size_t transmission_materials = 0;
        size_t exact_origin_instances = 0;
        size_t unresolved_exact_origin = 0;
        size_t unresolved_outside_ownership = 0;
        bool river_specular_color_preserved = false;
    };

    using SourceIndices =
        std::unordered_map<std::string, std::vector<uint32_t>>;

    bool append_package(
        const std::filesystem::path& path,
        Totals& totals,
        SourceIndices& source_indices,
        std::ostream& output,
        std::string& error_message);
}
