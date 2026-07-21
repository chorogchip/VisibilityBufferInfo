#include "scene/donut/DonutSceneDataCPU.h"

#include <limits>

namespace scene {

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
            output_error_message = "Donut vertex streams have different lengths.";
            return false;
        }

        if (indices.empty() || geometries.empty() || meshes.empty() ||
            instances.empty() || draws.empty() || materials.empty()) {
            output_error_message = "Donut scene is missing required geometry, instance, draw, or material data.";
            return false;
        }

        if (vertex_count > std::numeric_limits<uint32_t>::max() ||
            indices.size() > std::numeric_limits<uint32_t>::max()) {
            output_error_message = "Donut scene exceeds 32-bit buffer addressing.";
            return false;
        }

        for (const Geometry& geometry : geometries) {
            const uint64_t vertex_end = static_cast<uint64_t>(geometry.vertex_offset) + geometry.vertex_count;
            const uint64_t index_end = static_cast<uint64_t>(geometry.index_offset) + geometry.index_count;
            if (geometry.vertex_count == 0 || geometry.index_count == 0 ||
                vertex_end > vertex_count || index_end > indices.size() ||
                geometry.material_index >= materials.size()) {
                output_error_message = "Donut geometry references an invalid vertex, index, or material range.";
                return false;
            }

            for (uint32_t index = geometry.index_offset; index < index_end; ++index) {
                if (indices[index] >= geometry.vertex_count) {
                    output_error_message = "Donut geometry index is outside its local vertex range.";
                    return false;
                }
            }
        }

        for (const Mesh& mesh : meshes) {
            const uint64_t geometry_end = static_cast<uint64_t>(mesh.first_geometry) + mesh.geometry_count;
            if (mesh.geometry_count == 0 || geometry_end > geometries.size()) {
                output_error_message = "Donut mesh references an invalid geometry range.";
                return false;
            }
        }

        for (const Instance& instance : instances) {
            if (instance.mesh_index >= meshes.size()) {
                output_error_message = "Donut instance references an invalid mesh.";
                return false;
            }

            const Mesh& mesh = meshes[instance.mesh_index];
            const uint64_t draw_end =
                static_cast<uint64_t>(instance.first_geometry_instance) + mesh.geometry_count;
            if (draw_end > draws.size()) {
                output_error_message = "Donut instance references an invalid geometry-instance range.";
                return false;
            }
        }

        for (const Draw& draw : draws) {
            if (draw.geometry_index >= geometries.size() ||
                draw.instance_index >= instances.size()) {
                output_error_message = "Donut draw references an invalid geometry or instance.";
                return false;
            }

            const Mesh& mesh = meshes[instances[draw.instance_index].mesh_index];
            const uint64_t mesh_geometry_end =
                static_cast<uint64_t>(mesh.first_geometry) + mesh.geometry_count;
            if (draw.geometry_index < mesh.first_geometry ||
                draw.geometry_index >= mesh_geometry_end) {
                output_error_message = "Donut draw geometry does not belong to its instance mesh.";
                return false;
            }
        }

        return true;
    }
}
