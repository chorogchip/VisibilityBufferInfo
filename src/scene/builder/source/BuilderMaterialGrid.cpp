#include "scene/builder/source/BuilderMaterialGrid.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "util/Logger.h"
#include "math/MyDistribution.h"
#include "math/MyOrdering.h"
#include "math/MyRNG.h"
#include "scene/builder/source/SceneSourceDataValidator.h"

namespace scene {

    namespace {
        static source::Mesh build_mesh_(
            SceneSourceData& scene,
            const BuilderMaterialGrid::BuilderMaterialGridConfig& config);
    }

    std::unique_ptr<SceneSourceData> BuilderMaterialGrid::build(
        const BuilderMaterialGridConfig& config) {

        auto scene = std::make_unique<SceneSourceData>();

        scene->root_node_id = 0;
        scene->meshes.emplace_back(build_mesh_(*scene, config));

        scene->nodes.resize(2);
        scene->nodes[0].children.push_back(1);

        auto& node = scene->nodes[1];
        node.mesh_id = 0;
        node.first_instance = static_cast<uint32_t>(scene->instances.size());
        node.instance_count = 1;

        scene->instances.emplace_back();

        SceneSourceDataValidator::validate(*scene);
        return scene;
    }

    namespace {

        static source::Mesh build_mesh_(
            SceneSourceData& scene,
            const BuilderMaterialGrid::BuilderMaterialGridConfig& config) {

            source::Mesh ret{};

            const uint32_t material_count = (std::min)(
                config.material_count,
                config.triangle_division * config.triangle_division);
            const uint32_t material_class_count = (std::min)(
                config.material_class_count,
                material_count);

            if (material_count == 0 || material_class_count == 0) {
                return ret;
            }

            scene.materials.resize(material_count);
            scene.active_material_class_count = material_class_count;

            for (uint32_t i = 0; i < material_count; ++i) {
                scene.materials[i].virtual_shader_id =
                    i % material_class_count;
            }

            math::MyRNG rng(config.seed);

            const math::MyDistribution dist =
                math::MyDistribution::generate_diversed(
                    material_count, 1'000'000U, config.material_diversity);

            const std::vector<uint32_t> material_permutation =
                rng.generate_permutation(material_count);

            source::Primitive base_primitive{};

            const float inverse_division =
                1.0f / static_cast<float>(config.triangle_division);
            const float position_step = inverse_division * 2.0f;
            const DirectX::XMFLOAT3 normal = { 0.0f, 0.0f, -1.0f };

            for (uint32_t y = 0; y <= config.triangle_division; ++y) {
                const float fy = static_cast<float>(y);
                const float position_y = -1.0f + position_step * fy;

                for (uint32_t x = 0; x <= config.triangle_division; ++x) {
                    const float fx = static_cast<float>(x);
                    const float position_x = -1.0f + position_step * fx;

                    base_primitive.positions.push_back(
                        { position_x, position_y, 0.0f });
                    base_primitive.normals.push_back(normal);
                    base_primitive.uv0.push_back(
                        { fx * inverse_division, fy * inverse_division });
                }
            }

            std::vector<source::Primitive> primitives(
                material_count, base_primitive);

            for (uint32_t i = 0; i < material_count; ++i) {
                primitives[i].material_id = i;
            }

            const uint32_t row = config.triangle_division + 1;

            for (uint32_t y = 0; y < config.triangle_division; ++y) {
                for (uint32_t x = 0; x < config.triangle_division; ++x) {

                    const bool use_locality =
                        rng.sample_double() < config.material_locality;

                    const uint32_t logical_material_id =
                        use_locality ?
                        dist.sample_normalized(math::MyOrdering::z_order(x, y, config.triangle_division)) :
                        dist.sample(rng);

                    const uint32_t material_id =
                        material_permutation[logical_material_id];

                    source::Primitive& primitive =
                        primitives[material_id];

                    const uint32_t v00 = y * row + x;
                    const uint32_t v10 = y * row + x + 1;
                    const uint32_t v01 = (y + 1) * row + x;
                    const uint32_t v11 = (y + 1) * row + x + 1;

                    primitive.indices.push_back(v00);
                    primitive.indices.push_back(v01);
                    primitive.indices.push_back(v10);

                    primitive.indices.push_back(v10);
                    primitive.indices.push_back(v01);
                    primitive.indices.push_back(v11);
                }
            }

            for (source::Primitive& primitive : primitives) {
                if (!primitive.indices.empty()) {
                    ret.primitives.emplace_back(std::move(primitive));
                }
            }

            return ret;
        }
    }
}
