#include "JungleSceneSourceValidation.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <ostream>
#include <string>
#include <utility>

#include "JungleSceneSourceValidationInternal.h"

namespace jungle::validation {

    namespace {

        constexpr uint64_t EXPECTED_INSTANCES = 8'674'676;
        constexpr size_t EXPECTED_SOURCE_STREAMS = 778;
        constexpr size_t EXPECTED_INSTANCE_SETS = 969;
        constexpr size_t EXPECTED_CELLS = 81;
        constexpr size_t EXPECTED_SYSTEMS = 205;
        constexpr size_t EXPECTED_MESHES = 148;
        constexpr size_t EXPECTED_PRIMITIVES = 242;
        constexpr size_t EXPECTED_MATERIALS = 127;
        constexpr size_t EXPECTED_IMAGES = 281;
        constexpr size_t EXPECTED_CAMERAS = 1;
        constexpr size_t EXPECTED_UV1_PRIMITIVES = 19;
        constexpr size_t EXPECTED_COLOR0_PRIMITIVES = 101;
        constexpr size_t EXPECTED_COLOR1_PRIMITIVES = 101;
        constexpr size_t EXPECTED_ALPHA_BLEND_MATERIALS = 17;
        constexpr size_t EXPECTED_TRANSMISSION_MATERIALS = 12;
        constexpr size_t EXPECTED_EXACT_ORIGIN_INSTANCES = 197;
        constexpr size_t EXPECTED_UNRESOLVED_EXACT_ORIGIN = 5;
        constexpr size_t EXPECTED_UNRESOLVED_OUTSIDE_OWNERSHIP = 1;

        bool check_equal(
            uint64_t actual,
            uint64_t expected,
            const char* label,
            std::string& error_message) {

            if (actual == expected) return true;
            error_message =
                std::string(label) + " differs: expected " +
                std::to_string(expected) + ", got " +
                std::to_string(actual);
            return false;
        }

        bool validate_source_indices(
            SourceIndices& source_indices,
            std::string& error_message) {

            if (!check_equal(
                    source_indices.size(),
                    EXPECTED_SOURCE_STREAMS,
                    "source stream count",
                    error_message)) {
                return false;
            }

            for (auto& [key, indices] : source_indices) {
                std::sort(indices.begin(), indices.end());
                for (size_t index = 0; index < indices.size(); ++index) {
                    if (indices[index] != index) {
                        error_message =
                            "Source index coverage differs for " + key +
                            " at index " + std::to_string(index);
                        return false;
                    }
                }
            }
            return true;
        }

        bool validate_totals(
            const Totals& totals,
            std::string& error_message) {

            const std::pair<uint64_t, uint64_t> checks[] = {
                { totals.instances, EXPECTED_INSTANCES },
                { totals.instance_sets, EXPECTED_INSTANCE_SETS },
                { totals.cells, EXPECTED_CELLS },
                { totals.systems, EXPECTED_SYSTEMS },
                { totals.meshes, EXPECTED_MESHES },
                { totals.primitives, EXPECTED_PRIMITIVES },
                { totals.materials, EXPECTED_MATERIALS },
                { totals.images, EXPECTED_IMAGES },
                { totals.cameras, EXPECTED_CAMERAS },
                { totals.uv1_primitives, EXPECTED_UV1_PRIMITIVES },
                { totals.color0_primitives, EXPECTED_COLOR0_PRIMITIVES },
                { totals.color1_primitives, EXPECTED_COLOR1_PRIMITIVES },
                {
                    totals.alpha_blend_materials,
                    EXPECTED_ALPHA_BLEND_MATERIALS
                },
                {
                    totals.transmission_materials,
                    EXPECTED_TRANSMISSION_MATERIALS
                },
                {
                    totals.exact_origin_instances,
                    EXPECTED_EXACT_ORIGIN_INSTANCES
                },
                {
                    totals.unresolved_exact_origin,
                    EXPECTED_UNRESOLVED_EXACT_ORIGIN
                },
                {
                    totals.unresolved_outside_ownership,
                    EXPECTED_UNRESOLVED_OUTSIDE_OWNERSHIP
                }
            };
            const char* labels[] = {
                "instance count",
                "instance set count",
                "cell count",
                "system count",
                "mesh count",
                "primitive count",
                "material count",
                "image count",
                "camera count",
                "UV1 primitive count",
                "COLOR_0 primitive count",
                "COLOR_1 primitive count",
                "alpha blend material count",
                "transmission material count",
                "exact-origin instance count",
                "unresolved exact-origin instance count",
                "outside-ownership instance count"
            };

            for (size_t index = 0; index < std::size(checks); ++index) {
                if (!check_equal(
                        checks[index].first,
                        checks[index].second,
                        labels[index],
                        error_message)) {
                    return false;
                }
            }
            if (!totals.river_specular_color_preserved) {
                error_message =
                    "River KHR_materials_specular color was not preserved.";
                return false;
            }
            return true;
        }
    }

    bool validate_package_set(
        const std::vector<std::filesystem::path>& package_paths,
        std::ostream& output,
        std::string& error_message) {

        Totals totals{};
        SourceIndices source_indices;
        source_indices.reserve(EXPECTED_SOURCE_STREAMS);

        for (const std::filesystem::path& path : package_paths) {
            if (!append_package(
                    path,
                    totals,
                    source_indices,
                    output,
                    error_message)) {
                return false;
            }
        }
        if (!validate_source_indices(source_indices, error_message) ||
            !validate_totals(totals, error_message)) {
            return false;
        }

        output
            << "PASS: JungleSceneSourceBuilder preserved "
            << totals.instances << " instances across "
            << source_indices.size() << " source streams.\n"
            << "PASS: " << totals.primitives
            << " primitives, UV1=" << totals.uv1_primitives
            << ", COLOR_0=" << totals.color0_primitives
            << ", COLOR_1=" << totals.color1_primitives << ".\n"
            << "PASS: Jungle hierarchy, source identity, unresolved "
               "records, cameras, and River material metadata.\n";
        error_message = "ok";
        return true;
    }
}
