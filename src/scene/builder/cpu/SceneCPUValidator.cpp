#include "scene/builder/cpu/SceneCPUValidator.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "util/Logger.h"

namespace scene {

    void SceneCPUValidator::validate(const SceneCPUData& scene) {
        util::Logger::g_logger.assert_with_log(
            !scene.vertices.empty(),
            "CPU scene has no vertices.");
        util::Logger::g_logger.assert_with_log(
            !scene.indices.empty(),
            "CPU scene has no indices.");
        util::Logger::g_logger.assert_with_log(
            !scene.materials.empty(),
            "CPU scene has no materials.");
        util::Logger::g_logger.assert_with_log(
            !scene.submeshes.empty() && !scene.meshes.empty(),
            "CPU scene has no mesh data.");
        util::Logger::g_logger.assert_with_log(
            !scene.instances.empty(),
            "CPU scene has no instances.");
        util::Logger::g_logger.assert_with_log(
            !scene.nodes.empty() &&
            scene.root_node_id < scene.nodes.size(),
            "CPU scene has no valid node tree.");
        util::Logger::g_logger.assert_with_log(
            !scene.draw_instance_ids.empty() &&
            !scene.draw_calls.empty(),
            "CPU scene has no draw calls.");

        for (const SceneCPUData::Material& material : scene.materials) {
            const SceneCPUData::Material::TexturePath* paths[] = {
                &material.base_color_texture,
                &material.metal_roughness_texture,
                &material.normal_texture,
                &material.emissive_texture,
                &material.occlusion_texture
            };
            for (const SceneCPUData::Material::TexturePath* path : paths) {
                util::Logger::g_logger.assert_with_log(
                    !*path ||
                    (!(*path)->empty() && (*path)->is_absolute()),
                    "CPU scene texture paths must be non-empty absolute paths.");
            }
        }

        for (const SceneCPUData::Submesh& submesh : scene.submeshes) {
            const uint64_t vertex_end =
                static_cast<uint64_t>(submesh.vertex_offset) +
                submesh.vertex_count;
            const uint64_t index_end =
                static_cast<uint64_t>(submesh.index_offset) +
                submesh.index_count;
            util::Logger::g_logger.assert_with_log(
                submesh.vertex_count > 0 &&
                submesh.index_count > 0 &&
                submesh.index_count % 3 == 0 &&
                vertex_end <= scene.vertices.size() &&
                index_end <= scene.indices.size() &&
                submesh.material_id < scene.materials.size(),
                "CPU scene submesh range or material is invalid.");

            for (uint64_t index_id = submesh.index_offset; index_id < index_end; ++index_id) {
                util::Logger::g_logger.assert_with_log(
                    scene.indices[index_id] < submesh.vertex_count,
                    "CPU scene submesh has an out-of-range local index.");
            }
        }

        for (const SceneCPUData::Mesh& mesh : scene.meshes) {
            const uint64_t submesh_end =
                static_cast<uint64_t>(mesh.first_submesh) +
                mesh.submesh_count;
            util::Logger::g_logger.assert_with_log(
                mesh.submesh_count > 0 &&
                submesh_end <= scene.submeshes.size(),
                "CPU scene mesh has an invalid submesh range.");
        }

        for (const SceneCPUData::Instance& instance : scene.instances) {
            util::Logger::g_logger.assert_with_log(
                instance.mesh_id < scene.meshes.size(),
                "CPU scene instance references an invalid mesh.");
        }

        std::vector<uint8_t> visited(scene.nodes.size(), 0);
        std::vector<uint32_t> parent_counts(scene.nodes.size(), 0);
        std::vector<uint32_t> stack = { scene.root_node_id };
        while (!stack.empty()) {
            const uint32_t node_id = stack.back();
            stack.pop_back();
            util::Logger::g_logger.assert_with_log(
                node_id < scene.nodes.size() &&
                visited[node_id] == 0,
                "CPU scene node tree contains an invalid link or cycle.");
            visited[node_id] = 1;

            const SceneCPUData::Node& node = scene.nodes[node_id];
            util::Logger::g_logger.assert_with_log(
                node.mesh_id ==
                source::SceneConstants::INVALID_INDEX ||
                node.mesh_id < scene.meshes.size(),
                "CPU scene node references an invalid mesh.");
            util::Logger::g_logger.assert_with_log(
                node.first_instance <= scene.instances.size() &&
                node.instance_count <=
                scene.instances.size() - node.first_instance,
                "CPU scene node references an invalid instance range.");
            for (uint32_t instance_id = node.first_instance; instance_id < node.first_instance + node.instance_count; ++instance_id) {
                util::Logger::g_logger.assert_with_log(
                    node.mesh_id !=
                    source::SceneConstants::INVALID_INDEX &&
                    scene.instances[instance_id].mesh_id ==
                    node.mesh_id,
                    "CPU scene node instance does not use the node mesh.");
            }

            for (uint32_t child_id : node.children) {
                util::Logger::g_logger.assert_with_log(
                    child_id < scene.nodes.size() &&
                    child_id != node_id,
                    "CPU scene node has an invalid child.");
                ++parent_counts[child_id];
                util::Logger::g_logger.assert_with_log(
                    parent_counts[child_id] == 1,
                    "CPU scene node has more than one parent.");
                stack.push_back(child_id);
            }
        }
        util::Logger::g_logger.assert_with_log(
            std::all_of(
                visited.begin(),
                visited.end(),
                [](uint8_t value) { return value == 1; }),
            "CPU scene contains a node outside the root tree.");

        for (uint32_t instance_id : scene.draw_instance_ids) {
            util::Logger::g_logger.assert_with_log(
                instance_id < scene.instances.size(),
                "CPU scene draw stream references an invalid instance.");
        }

        for (const SceneCPUData::DrawCall& draw : scene.draw_calls) {
            const uint64_t instance_end =
                static_cast<uint64_t>(draw.first_instance) +
                draw.instance_count;
            util::Logger::g_logger.assert_with_log(
                draw.instance_count > 0 &&
                draw.submesh_id < scene.submeshes.size() &&
                instance_end <= scene.draw_instance_ids.size(),
                "CPU scene draw call has an invalid range.");

            const SceneCPUData::Submesh& submesh =
                scene.submeshes[draw.submesh_id];
            util::Logger::g_logger.assert_with_log(
                draw.index_count == submesh.index_count &&
                draw.index_offset == submesh.index_offset &&
                draw.vertex_offset == submesh.vertex_offset &&
                draw.material_id == submesh.material_id,
                "CPU scene draw call disagrees with its submesh.");
        }
    }
}
