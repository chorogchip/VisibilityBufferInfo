#include "scene/data/source/SceneSourceData.h"

#include <algorithm>
#include <string>
#include <vector>

#include "util/Logger.h"

namespace scene {

    void SceneSourceData::validate() const {
        util::Logger::g_logger.assert_with_log(
            !nodes.empty(),
            "Scene source has no nodes.");
        util::Logger::g_logger.assert_with_log(
            !meshes.empty(),
            "Scene source has no meshes.");

        for (const source::Node& node : nodes) {
            node.validate();
        }
        for (const source::Mesh& mesh : meshes) {
            mesh.validate();
        }

        this->validate_hierarchy_();
        this->validate_references_();
    }

    void SceneSourceData::validate_hierarchy_() const {
        util::Logger::g_logger.assert_with_log(
            root_node_id < nodes.size(),
            "Scene source has an invalid root node.");

        std::vector<uint8_t> visited(nodes.size(), 0);
        std::vector<uint32_t> parent_counts(nodes.size(), 0);
        std::vector<uint32_t> stack = { root_node_id };

        while (!stack.empty()) {
            const uint32_t node_id = stack.back();
            stack.pop_back();

            util::Logger::g_logger.assert_with_log(
                node_id < nodes.size(),
                "Scene source hierarchy contains an invalid node ID.");
            util::Logger::g_logger.assert_with_log(
                visited[node_id] == 0,
                "Scene source hierarchy contains a cycle.");
            visited[node_id] = 1;

            for (uint32_t child_id : nodes[node_id].children) {
                util::Logger::g_logger.assert_with_log(
                    child_id < nodes.size() && child_id != node_id,
                    "Scene source hierarchy contains an invalid child ID.");
                ++parent_counts[child_id];
                util::Logger::g_logger.assert_with_log(
                    parent_counts[child_id] == 1,
                    "Scene source node has more than one parent.");
                stack.push_back(child_id);
            }
        }

        util::Logger::g_logger.assert_with_log(
            std::all_of(
                visited.begin(),
                visited.end(),
                [](uint8_t value) { return value == 1; }),
            "Scene source contains a node outside the root hierarchy.");
    }

    void SceneSourceData::validate_references_() const {
        for (size_t node_id = 0; node_id < nodes.size(); ++node_id) {
            const source::Node& node = nodes[node_id];
            const uint32_t mesh_id = node.mesh_id;
            util::Logger::g_logger.assert_with_log(
                mesh_id == source::SceneConstants::INVALID_INDEX ||
                mesh_id < meshes.size(),
                ("Scene source node " + std::to_string(node_id) +
                    " references an invalid mesh.").c_str());
            util::Logger::g_logger.assert_with_log(
                node.camera_id == source::SceneConstants::INVALID_INDEX ||
                node.camera_id < cameras.size(),
                ("Scene source node " + std::to_string(node_id) +
                    " references an invalid camera.").c_str());
            util::Logger::g_logger.assert_with_log(
                node.first_instance <= instances.size() &&
                node.instance_count <=
                instances.size() - node.first_instance,
                ("Scene source node " + std::to_string(node_id) +
                    " references an invalid instance range.").c_str());
        }

        for (size_t mesh_id = 0; mesh_id < meshes.size(); ++mesh_id) {
            const source::Mesh& mesh = meshes[mesh_id];
            for (size_t primitive_id = 0; primitive_id < mesh.primitives.size(); ++primitive_id) {
                const uint32_t material_id =
                    mesh.primitives[primitive_id].material_id;
                util::Logger::g_logger.assert_with_log(
                    material_id == source::SceneConstants::INVALID_INDEX ||
                    material_id < materials.size(),
                    ("Scene source mesh " + std::to_string(mesh_id) +
                        " primitive " + std::to_string(primitive_id) +
                        " references an invalid material.").c_str());
            }
        }

        for (const source::Texture& texture : textures) {
            util::Logger::g_logger.assert_with_log(
                texture.image_id < images.size(),
                "Scene source texture references an invalid image.");
            util::Logger::g_logger.assert_with_log(
                texture.sampler_id == source::SceneConstants::INVALID_INDEX ||
                texture.sampler_id < samplers.size(),
                "Scene source texture references an invalid sampler.");
        }

        const auto validate_texture_ref =
            [this](const source::TextureRef& texture) {
            util::Logger::g_logger.assert_with_log(
                !texture.valid() || texture.texture_id < textures.size(),
                "Scene source material references an invalid texture.");
        };
        for (const source::Material& material : materials) {
            validate_texture_ref(material.base_color_texture);
            validate_texture_ref(material.metal_roughness_texture);
            validate_texture_ref(material.normal_texture);
            validate_texture_ref(material.emissive_texture);
            validate_texture_ref(material.occlusion_texture);
            validate_texture_ref(material.transmission_texture);
        }
    }
}
