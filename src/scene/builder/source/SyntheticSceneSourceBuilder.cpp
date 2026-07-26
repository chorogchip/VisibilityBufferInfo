#include "scene/builder/source/SyntheticSceneSourceBuilder.h"

#include <utility>
#include <vector>

#include <DirectXMath.h>

#include "util/Logger.h"

namespace scene {

    SyntheticSceneSourceBuilder::SyntheticSceneSourceBuilder(
        const SyntheticPlaneConfig& config)
        : config_(config) {
    }

    SceneSourceData SyntheticSceneSourceBuilder::build() const {
        util::Logger::g_logger.assert_with_log(
            config_.object_count > 0,
            "Synthetic plane object count must be greater than zero.");
        util::Logger::g_logger.assert_with_log(
            config_.division > 0,
            "Synthetic plane division must be greater than zero.");
        util::Logger::g_logger.assert_with_log(
            config_.overdraw_count < config_.object_count,
            "Synthetic plane overdraw count must be less than object count.");

        SceneSourceData scene{};
        scene.root_node_id = 0;
        scene.materials.emplace_back();

        source::Mesh mesh{};
        mesh.primitives.emplace_back(this->build_primitive_());
        scene.meshes.emplace_back(std::move(mesh));

        scene.nodes.resize(2);
        scene.nodes[0].children.push_back(1);
        scene.nodes[1].mesh_id = 0;
        this->build_instances_(scene, scene.nodes[1]);

        scene.validate();
        return scene;
    }

    source::Primitive SyntheticSceneSourceBuilder::build_primitive_() const {
        source::Primitive primitive{};
        primitive.material_id = 0;

        const float inverse_division =
            1.0f / static_cast<float>(config_.division);
        const float position_step = inverse_division * 2.0f;
        const DirectX::XMFLOAT3 normal = { 0.0f, 0.0f, -1.0f };

        for (uint32_t y = 0; y <= config_.division; ++y) {
            const float fy = static_cast<float>(y);
            const float position_y = -1.0f + position_step * fy;
            for (uint32_t x = 0; x <= config_.division; ++x) {
                const float fx = static_cast<float>(x);
                const float position_x = -1.0f + position_step * fx;
                primitive.positions.push_back(
                    { position_x, position_y, 0.0f });
                primitive.normals.push_back(normal);
                primitive.uv0.push_back(
                    { fx * inverse_division, fy * inverse_division });
            }
        }

        const uint32_t row = config_.division + 1;
        for (uint32_t y = 0; y < config_.division; ++y) {
            for (uint32_t x = 0; x < config_.division; ++x) {
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

        return primitive;
    }

    void SyntheticSceneSourceBuilder::build_instances_(
        SceneSourceData& scene,
        source::Node& node) const {
        std::vector<uint32_t> order;
        order.reserve(config_.object_count);

        uint32_t object_id = 0;
        for (; object_id < config_.overdraw_count + 1; ++object_id) {
            order.push_back(config_.overdraw_count - object_id);
        }
        for (; object_id < config_.object_count; ++object_id) {
            order.push_back(object_id);
        }

        node.first_instance =
            static_cast<uint32_t>(scene.instances.size());
        scene.instances.reserve(
            scene.instances.size() + config_.object_count);
        for (uint32_t i = 0; i < config_.object_count; ++i) {
            float position_z_offset = 0.0f;
            if (i != 0 && config_.to_remain_only_in_camera) {
                position_z_offset = -1000.0f;
            }

            const float position_z =
                position_z_offset +
                static_cast<float>(order[i]) /
                static_cast<float>(config_.object_count);
            source::InstanceTransform transform{};
            transform.translation.z = position_z;
            transform.source_index = i;
            scene.instances.push_back(transform);
        }
        node.instance_count = config_.object_count;
    }
}
