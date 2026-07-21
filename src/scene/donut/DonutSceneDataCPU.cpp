#include "scene/donut/DonutSceneDataCPU.h"

#include <algorithm>
#include <limits>

namespace scene {

    namespace {

        bool is_invalid(uint32_t value) {
            return value == DonutSceneDataCPU::INVALID_INDEX;
        }
    }

    bool DonutSceneDataCPU::validate(std::string& output_error_message) const {
        const size_t vertex_count = positions.size();
        if (vertex_count == 0) {
            output_error_message = "Donut scene has no vertices.";
            return false;
        }

        if (prev_positions.size() != vertex_count ||
            texcoords.size() != vertex_count ||
            packed_normals.size() != vertex_count ||
            packed_tangents.size() != vertex_count) {
            output_error_message = "Donut scene vertex streams have different lengths.";
            return false;
        }

        if (indices.empty() || nodes.empty() || instances.empty() ||
            meshes.empty() || submeshes.empty() || geometry_instances.empty() ||
            materials.empty()) {
            output_error_message = "Donut scene is missing required render data.";
            return false;
        }

        if (root_node_id >= nodes.size()) {
            output_error_message = "Donut scene root node is invalid.";
            return false;
        }

        if (vertex_count > (std::numeric_limits<uint32_t>::max)() ||
            indices.size() > (std::numeric_limits<uint32_t>::max)()) {
            output_error_message = "Donut scene exceeds 32-bit buffer addressing.";
            return false;
        }

        for (uint32_t node_id = 0; node_id < nodes.size(); ++node_id) {
            const Node& node = nodes[node_id];
            if (node_id == root_node_id) {
                if (!is_invalid(node.parent_id)) {
                    output_error_message = "Donut scene root node has a parent.";
                    return false;
                }
            } else if (node.parent_id >= nodes.size()) {
                output_error_message = "Donut scene node references an invalid parent.";
                return false;
            }

            if (!is_invalid(node.instance_id) && node.instance_id >= instances.size()) {
                output_error_message = "Donut scene node references an invalid instance.";
                return false;
            }

            for (uint32_t child_id : node.children) {
                if (child_id >= nodes.size() || nodes[child_id].parent_id != node_id) {
                    output_error_message = "Donut scene node child link is invalid.";
                    return false;
                }
            }
        }

        for (const Texture& texture : textures) {
            if (!texture.embedded && texture.path.empty()) {
                output_error_message = "Donut scene texture has no external path.";
                return false;
            }
        }

        for (const Material& material : materials) {
            for (uint32_t texture_id : material.texture_ids) {
                if (!is_invalid(texture_id) && texture_id >= textures.size()) {
                    output_error_message = "Donut scene material references an invalid texture.";
                    return false;
                }
            }
        }

        for (const Submesh& submesh : submeshes) {
            const uint64_t vertex_end =
                static_cast<uint64_t>(submesh.vertex_offset) + submesh.vertex_count;
            const uint64_t index_end =
                static_cast<uint64_t>(submesh.index_offset) + submesh.index_count;

            if (submesh.vertex_count == 0 || submesh.index_count == 0 ||
                submesh.index_count % 3 != 0 ||
                vertex_end > vertex_count || index_end > indices.size() ||
                submesh.material_id >= materials.size()) {
                output_error_message = "Donut scene submesh range or material is invalid.";
                return false;
            }

            for (uint32_t index = submesh.index_offset; index < index_end; ++index) {
                if (indices[index] >= submesh.vertex_count) {
                    output_error_message = "Donut scene submesh index is outside its local vertex range.";
                    return false;
                }
            }
        }

        for (const Mesh& mesh : meshes) {
            const uint64_t submesh_end =
                static_cast<uint64_t>(mesh.first_submesh) + mesh.submesh_count;
            if (mesh.submesh_count == 0 || submesh_end > submeshes.size()) {
                output_error_message = "Donut scene mesh references an invalid submesh range.";
                return false;
            }
        }

        for (uint32_t instance_id = 0; instance_id < instances.size(); ++instance_id) {
            const Instance& instance = instances[instance_id];
            if (instance.node_id >= nodes.size() || instance.mesh_id >= meshes.size()) {
                output_error_message = "Donut scene instance references an invalid node or mesh.";
                return false;
            }

            if (nodes[instance.node_id].instance_id != instance_id) {
                output_error_message = "Donut scene node and instance links disagree.";
                return false;
            }

            const Mesh& mesh = meshes[instance.mesh_id];
            const uint64_t geometry_instance_end =
                static_cast<uint64_t>(instance.first_geometry_instance) +
                instance.geometry_instance_count;
            if (instance.geometry_instance_count != mesh.submesh_count ||
                geometry_instance_end > geometry_instances.size()) {
                output_error_message = "Donut scene instance has an invalid geometry-instance range.";
                return false;
            }
        }

        for (uint32_t geometry_instance_id = 0;
            geometry_instance_id < geometry_instances.size();
            ++geometry_instance_id) {
            const GeometryInstance& geometry_instance =
                geometry_instances[geometry_instance_id];
            if (geometry_instance.instance_id >= instances.size() ||
                geometry_instance.submesh_id >= submeshes.size()) {
                output_error_message = "Donut scene geometry instance references invalid data.";
                return false;
            }

            const Instance& instance = instances[geometry_instance.instance_id];
            const Mesh& mesh = meshes[instance.mesh_id];
            const uint32_t submesh_end = mesh.first_submesh + mesh.submesh_count;
            if (geometry_instance.submesh_id < mesh.first_submesh ||
                geometry_instance.submesh_id >= submesh_end) {
                output_error_message = "Donut scene geometry instance submesh does not belong to its mesh.";
                return false;
            }

            const uint32_t local_geometry_instance_id =
                geometry_instance_id - instance.first_geometry_instance;
            if (geometry_instance_id < instance.first_geometry_instance ||
                local_geometry_instance_id >= instance.geometry_instance_count) {
                output_error_message = "Donut scene geometry instance is outside its instance range.";
                return false;
            }
        }

        return true;
    }

    void DonutSceneDataCPU::build_all_visible_draws(
        std::vector<VisibleDraw>& output) const {
        output.clear();
        output.reserve(geometry_instances.size());
        for (uint32_t geometry_instance_id = 0;
            geometry_instance_id < geometry_instances.size();
            ++geometry_instance_id) {
            output.push_back({ geometry_instance_id });
        }
    }

    void DonutSceneDataCPU::sort_visible_draws(
        std::vector<VisibleDraw>& draws) const {
        std::sort(draws.begin(), draws.end(),
            [this](const VisibleDraw& lhs, const VisibleDraw& rhs) {
                const GeometryInstance& lhs_geometry =
                    geometry_instances[lhs.geometry_instance_id];
                const GeometryInstance& rhs_geometry =
                    geometry_instances[rhs.geometry_instance_id];
                const Submesh& lhs_submesh = submeshes[lhs_geometry.submesh_id];
                const Submesh& rhs_submesh = submeshes[rhs_geometry.submesh_id];

                if (lhs_submesh.material_id != rhs_submesh.material_id)
                    return lhs_submesh.material_id < rhs_submesh.material_id;
                if (lhs_geometry.submesh_id != rhs_geometry.submesh_id)
                    return lhs_geometry.submesh_id < rhs_geometry.submesh_id;
                return lhs_geometry.instance_id < rhs_geometry.instance_id;
            });
    }
}
