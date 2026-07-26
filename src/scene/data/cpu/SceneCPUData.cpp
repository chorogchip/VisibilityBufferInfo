#include "scene/data/cpu/SceneCPUData.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "util/Logger.h"

namespace scene {

    void SceneCPUData::validate() const {
        util::Logger::g_logger.assert_with_log(
            !vertices.empty(),
            "CPU scene has no vertices.");
        util::Logger::g_logger.assert_with_log(
            !indices.empty(),
            "CPU scene has no indices.");
        util::Logger::g_logger.assert_with_log(
            !materials.empty(),
            "CPU scene has no materials.");
        util::Logger::g_logger.assert_with_log(
            !submeshes.empty() && !meshes.empty(),
            "CPU scene has no mesh data.");
        util::Logger::g_logger.assert_with_log(
            !instances.empty(),
            "CPU scene has no instances.");
        util::Logger::g_logger.assert_with_log(
            !nodes.empty() && root_node_id < nodes.size(),
            "CPU scene has no valid node tree.");
        util::Logger::g_logger.assert_with_log(
            !draw_instance_ids.empty() && !draw_calls.empty(),
            "CPU scene has no draw calls.");

        for (const Material& material : materials) {
            const Material::TexturePath* texture_paths[] = {
                &material.base_color_texture,
                &material.metal_roughness_texture,
                &material.normal_texture,
                &material.emissive_texture,
                &material.occlusion_texture
            };
            for (const Material::TexturePath* texture_path : texture_paths) {
                util::Logger::g_logger.assert_with_log(
                    !*texture_path ||
                    (!(*texture_path)->empty() && (*texture_path)->is_absolute()),
                    "CPU scene texture paths must be non-empty absolute paths.");
            }
        }

        for (const Submesh& submesh : submeshes) {
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
                vertex_end <= vertices.size() &&
                index_end <= indices.size() &&
                submesh.material_id < materials.size(),
                "CPU scene submesh range or material is invalid.");

            for (uint64_t i = submesh.index_offset; i < index_end; ++i) {
                util::Logger::g_logger.assert_with_log(
                    indices[i] < submesh.vertex_count,
                    "CPU scene submesh has an out-of-range local index.");
            }
        }

        for (const Mesh& mesh : meshes) {
            const uint64_t submesh_end =
                static_cast<uint64_t>(mesh.first_submesh) +
                mesh.submesh_count;
            util::Logger::g_logger.assert_with_log(
                mesh.submesh_count > 0 && submesh_end <= submeshes.size(),
                "CPU scene mesh has an invalid submesh range.");
        }

        for (const Instance& instance : instances) {
            util::Logger::g_logger.assert_with_log(
                instance.mesh_id < meshes.size(),
                "CPU scene instance references an invalid mesh.");
        }

        std::vector<uint8_t> visited(nodes.size(), 0);
        std::vector<uint32_t> parent_counts(nodes.size(), 0);
        std::vector<uint32_t> stack = { root_node_id };
        while (!stack.empty()) {
            const uint32_t node_id = stack.back();
            stack.pop_back();
            util::Logger::g_logger.assert_with_log(
                node_id < nodes.size() && visited[node_id] == 0,
                "CPU scene node tree contains an invalid link or cycle.");
            visited[node_id] = 1;

            const Node& node = nodes[node_id];
            util::Logger::g_logger.assert_with_log(
                node.mesh_id == source::SceneConstants::INVALID_INDEX ||
                node.mesh_id < meshes.size(),
                "CPU scene node references an invalid mesh.");
            util::Logger::g_logger.assert_with_log(
                node.first_instance <= instances.size() &&
                node.instance_count <= instances.size() - node.first_instance,
                "CPU scene node references an invalid instance range.");
            for (uint32_t instance_id = node.first_instance; instance_id < node.first_instance + node.instance_count; ++instance_id) {
                util::Logger::g_logger.assert_with_log(
                    node.mesh_id != source::SceneConstants::INVALID_INDEX &&
                    instances[instance_id].mesh_id == node.mesh_id,
                    "CPU scene node instance does not use the node mesh.");
            }

            for (uint32_t child_id : node.children) {
                util::Logger::g_logger.assert_with_log(
                    child_id < nodes.size() && child_id != node_id,
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

        for (uint32_t instance_id : draw_instance_ids) {
            util::Logger::g_logger.assert_with_log(
                instance_id < instances.size(),
                "CPU scene draw stream references an invalid instance.");
        }

        for (const DrawCall& draw : draw_calls) {
            const uint64_t instance_end =
                static_cast<uint64_t>(draw.first_instance) +
                draw.instance_count;
            util::Logger::g_logger.assert_with_log(
                draw.instance_count > 0 &&
                draw.submesh_id < submeshes.size() &&
                instance_end <= draw_instance_ids.size(),
                "CPU scene draw call has an invalid range.");

            const Submesh& submesh = submeshes[draw.submesh_id];
            util::Logger::g_logger.assert_with_log(
                draw.index_count == submesh.index_count &&
                draw.index_offset == submesh.index_offset &&
                draw.vertex_offset == submesh.vertex_offset &&
                draw.material_id == submesh.material_id,
                "CPU scene draw call disagrees with its submesh.");
        }
    }
}
