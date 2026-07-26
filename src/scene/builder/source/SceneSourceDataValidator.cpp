#include "scene/builder/source/SceneSourceDataValidator.h"

#include <cstddef>
#include <string>

#include "scene/builder/source/SceneSourceGeometryValidator.h"
#include "scene/builder/source/SceneSourceHierarchyValidator.h"
#include "util/Logger.h"

namespace scene {

    void SceneSourceDataValidator::validate(
        const SceneSourceData& scene) {

        util::Logger::g_logger.assert_with_log(
            !scene.nodes.empty(),
            "Scene source has no nodes.");
        util::Logger::g_logger.assert_with_log(
            !scene.meshes.empty(),
            "Scene source has no meshes.");

        for (const source::Node& node : scene.nodes) {
            SceneSourceHierarchyValidator::validate(node);
        }
        for (const source::Mesh& mesh : scene.meshes) {
            SceneSourceGeometryValidator::validate(mesh);
        }
        SceneSourceHierarchyValidator::validate(scene);

        for (size_t node_id = 0; node_id < scene.nodes.size(); ++node_id) {
            const source::Node& node = scene.nodes[node_id];
            util::Logger::g_logger.assert_with_log(
                node.mesh_id ==
                source::SceneConstants::INVALID_INDEX ||
                node.mesh_id < scene.meshes.size(),
                ("Scene source node " +
                    std::to_string(node_id) +
                    " references an invalid mesh.").c_str());
            util::Logger::g_logger.assert_with_log(
                node.camera_id ==
                source::SceneConstants::INVALID_INDEX ||
                node.camera_id < scene.cameras.size(),
                ("Scene source node " +
                    std::to_string(node_id) +
                    " references an invalid camera.").c_str());
            util::Logger::g_logger.assert_with_log(
                node.first_instance <= scene.instances.size() &&
                node.instance_count <=
                scene.instances.size() - node.first_instance,
                ("Scene source node " +
                    std::to_string(node_id) +
                    " references an invalid instance range.").c_str());
        }

        for (size_t mesh_id = 0; mesh_id < scene.meshes.size(); ++mesh_id) {
            const source::Mesh& mesh = scene.meshes[mesh_id];
            for (size_t primitive_id = 0; primitive_id < mesh.primitives.size(); ++primitive_id) {
                const uint32_t material_id =
                    mesh.primitives[primitive_id].material_id;
                util::Logger::g_logger.assert_with_log(
                    material_id ==
                    source::SceneConstants::INVALID_INDEX ||
                    material_id < scene.materials.size(),
                    ("Scene source mesh " +
                        std::to_string(mesh_id) +
                        " primitive " +
                        std::to_string(primitive_id) +
                        " references an invalid material.").c_str());
            }
        }

        for (const source::Texture& texture : scene.textures) {
            util::Logger::g_logger.assert_with_log(
                texture.image_id < scene.images.size(),
                "Scene source texture references an invalid image.");
            util::Logger::g_logger.assert_with_log(
                texture.sampler_id ==
                source::SceneConstants::INVALID_INDEX ||
                texture.sampler_id < scene.samplers.size(),
                "Scene source texture references an invalid sampler.");
        }

        const auto validate_texture =
            [&scene](const source::TextureRef& texture) {
            util::Logger::g_logger.assert_with_log(
                !texture.valid() ||
                texture.texture_id < scene.textures.size(),
                "Scene source material references an invalid texture.");
        };
        for (const source::Material& material : scene.materials) {
            validate_texture(material.base_color_texture);
            validate_texture(material.metal_roughness_texture);
            validate_texture(material.normal_texture);
            validate_texture(material.emissive_texture);
            validate_texture(material.occlusion_texture);
            validate_texture(material.transmission_texture);
        }
    }
}
