#include "scene/builder/source/VirtualMaterialClassBuilder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>
#include <limits>

#include "util/Logger.h"
#include "math/MyRNG.h"
#include "math/MyDistribution.h"

namespace scene {

    namespace {

        void assign_random(
            SceneSourceData& scene,
            const VirtualMaterialClassBuilder::Params& params) {

            const uint32_t active_bin_count =
                static_cast<uint32_t>((std::min)(
                    scene.materials.size(),
                    static_cast<size_t>(params.max_material_real_open)));

            if (active_bin_count == 0) return;

            math::MyRNG rng(params.seed);

            const std::vector<uint32_t> bin_permutation =
                rng.generate_permutation(active_bin_count);

            const math::MyDistribution distribution =
                math::MyDistribution::generate_diversed(active_bin_count, 1'000'000U, params.material_diversity);

            for (auto& material : scene.materials) {
                const uint32_t logical_bin = distribution.sample(rng);

                material.virtual_shader_id =
                    bin_permutation[logical_bin];
            }

            scene.active_material_class_count = active_bin_count;
        }

        void assign_pbr_features(
            SceneSourceData& scene,
            const VirtualMaterialClassBuilder::Params& params) {

            const uint32_t mat_sz = static_cast<uint32_t>(scene.materials.size());
            std::vector<std::pair<uint32_t,uint32_t>> mat_vec;
            mat_vec.reserve(mat_sz + 1);

            for (uint32_t i = 0; i < mat_sz; ++i) {
                auto& material = scene.materials[i];
                uint32_t key = 0;

                key |= (1U & material.base_color_texture.valid()) << 0U;
                key |= (1U & material.metal_roughness_texture.valid()) << 1U;
                key |= (1U & material.normal_texture.valid()) << 2U;
                key |= (1U & material.emissive_texture.valid()) << 3U;
                key |= (1U & material.occlusion_texture.valid()) << 4U;
                key |= (1U & (material.transmission_texture.valid() ||
                    material.transmission > 0.0f)) << 5U;
                key |= (1U & material.double_sided) << 6U;
                key |= (1U & material.alpha_mode ==
                    source::AlphaMode::Mask) << 7U;
                key |= (1U & material.alpha_mode ==
                    source::AlphaMode::Blend) << 8U;

                mat_vec.emplace_back(key, i);
            }
            
            std::sort(mat_vec.begin(), mat_vec.end());
            mat_vec.emplace_back(std::numeric_limits<uint32_t>::max(), 0);

            uint32_t mat_bin = 0;
            for (uint32_t i = 0; i < mat_sz; ++i) {
                scene.materials[mat_vec[i].second].virtual_shader_id = mat_bin;

                if (mat_vec[i].first != mat_vec[i + 1].first)
                    ++mat_bin;
            }
            
            util::Logger::g_logger.assert_with_log(
                mat_bin <= params.max_material_bin_limit,
                "virtual material based on pbr exceeds limit");
            util::Logger::g_logger.assert_with_log(
                mat_bin <= params.max_material_real_open,
                "virtual material based on pbr exceeds real limit");

            scene.active_material_class_count = mat_bin;
        }
    }

    void VirtualMaterialClassBuilder::assign_materials(
        SceneSourceData& scene,
        const Params& params) {

        if (params.strategy == EnumAssignStrategy::RANDOM) {
            assign_random(scene, params);
        } else if (params.strategy == EnumAssignStrategy::PBR_FEATURE) {
            assign_pbr_features(scene, params);
        }
    }
}
