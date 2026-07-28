#include "scene/builder/source/SceneSourceSemanticValidator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <string>
#include <unordered_set>

#include "scene/data/source/SceneConstants.h"
#include "util/Logger.h"

namespace scene {

    namespace {

        bool is_finite_bounds_(const math::AABB& bounds) {
            if (!bounds.is_valid) {
                return true;
            }
            const float values[] = {
                bounds.pos_min.x, bounds.pos_min.y, bounds.pos_min.z,
                bounds.pos_max.x, bounds.pos_max.y, bounds.pos_max.z,
            };
            return std::all_of(
                std::begin(values),
                std::end(values),
                [](float value) { return std::isfinite(value); });
        }

        void validate_reference_(
            const source::SourceReference& reference,
            const char* object_name) {
            util::Logger::g_logger.assert_with_log(
                !reference.stable_id.empty() && !reference.prim_path.empty(),
                (std::string(object_name) +
                    " is missing a stable USD source reference.").c_str());
        }

        uint64_t primvar_value_count_(const source::Primvar& primvar) {
            if (primvar.numeric_component_count == 0) {
                return 0;
            }
            const uint64_t component_count = primvar.numeric_component_count;
            if (!primvar.float_values.empty()) {
                return primvar.float_values.size() / component_count;
            }
            if (!primvar.double_values.empty()) {
                return primvar.double_values.size() / component_count;
            }
            if (!primvar.integer_values.empty()) {
                return primvar.integer_values.size() / component_count;
            }
            return 0;
        }

    } // namespace

    void SceneSourceSemanticValidator::validate(const SceneSourceData& scene) {
        util::Logger::g_logger.assert_with_log(
            !scene.nodes.empty() && scene.root_node_id < scene.nodes.size(),
            "Semantic scene source has no valid root node.");
        util::Logger::g_logger.assert_with_log(
            is_finite_bounds_(scene.metadata.source_bounds),
            "Semantic scene source has non-finite bounds.");

        std::vector<uint32_t> parent_counts(scene.nodes.size(), 0);
        std::vector<uint8_t> visited(scene.nodes.size(), 0);
        std::vector<uint32_t> stack = { scene.root_node_id };
        while (!stack.empty()) {
            const uint32_t node_id = stack.back();
            stack.pop_back();
            util::Logger::g_logger.assert_with_log(
                node_id < scene.nodes.size() && visited[node_id] == 0,
                "Semantic scene hierarchy contains an invalid cycle.");
            visited[node_id] = 1;
            const auto& node = scene.nodes[node_id];
            validate_reference_(node.source, "SceneSource node");
            util::Logger::g_logger.assert_with_log(
                is_finite_bounds_(node.world_bounds),
                "Semantic scene node has non-finite bounds.");
            for (const uint32_t child_id : node.children) {
                util::Logger::g_logger.assert_with_log(
                    child_id < scene.nodes.size() && child_id != node_id,
                    "Semantic scene hierarchy has an invalid child.");
                ++parent_counts[child_id];
                util::Logger::g_logger.assert_with_log(
                    parent_counts[child_id] == 1 &&
                    scene.nodes[child_id].parent_node_id == node_id,
                    "Semantic scene hierarchy has an invalid parent relation.");
                stack.push_back(child_id);
            }
        }
        util::Logger::g_logger.assert_with_log(
            std::all_of(visited.begin(), visited.end(), [](uint8_t value) {
                return value == 1;
            }),
            "Semantic scene has nodes outside the root hierarchy.");

        std::unordered_set<std::string> prototype_ids;
        for (const auto& prototype : scene.native_prototypes) {
            validate_reference_(prototype.source, "Native prototype");
            util::Logger::g_logger.assert_with_log(
                prototype.root_node_id < scene.nodes.size() &&
                prototype_ids.insert(prototype.source.stable_id).second,
                "Native prototype has an invalid or duplicate source ID.");
        }
        for (const auto& instance : scene.native_instances) {
            validate_reference_(instance.source, "Native instance");
            util::Logger::g_logger.assert_with_log(
                instance.node_id < scene.nodes.size() &&
                prototype_ids.contains(instance.prototype_source_id),
                "Native instance references an invalid prototype.");
        }

        for (const auto& mesh : scene.polygon_meshes) {
            validate_reference_(mesh.source, "Polygon mesh");
            util::Logger::g_logger.assert_with_log(
                is_finite_bounds_(mesh.local_bounds),
                "Polygon mesh has non-finite bounds.");
            const uint64_t face_vertex_count = std::accumulate(
                mesh.face_vertex_counts.begin(),
                mesh.face_vertex_counts.end(),
                uint64_t{ 0 });
            util::Logger::g_logger.assert_with_log(
                face_vertex_count == mesh.face_vertex_indices.size(),
                "Polygon mesh face counts do not match face indices.");
            for (const auto index : mesh.face_vertex_indices) {
                util::Logger::g_logger.assert_with_log(
                    index < mesh.points.size(),
                    "Polygon mesh contains an out-of-range point index.");
            }
            for (const auto& primvar : mesh.primvars) {
                validate_reference_(primvar.source, "Primvar");
                const uint64_t value_count = primvar_value_count_(primvar);
                for (const auto index : primvar.indices) {
                    util::Logger::g_logger.assert_with_log(
                        value_count == 0 || index < value_count,
                        "Indexed primvar contains an out-of-range value index.");
                }
            }
            for (const auto& subset : mesh.material_subsets) {
                validate_reference_(subset.source, "Material subset");
                for (const auto face_index : subset.face_indices) {
                    util::Logger::g_logger.assert_with_log(
                        face_index < mesh.face_vertex_counts.size(),
                        "Material subset contains an out-of-range face index.");
                }
            }
        }

        for (const auto& instancer : scene.point_instancers) {
            validate_reference_(instancer.source, "PointInstancer");
            util::Logger::g_logger.assert_with_log(
                instancer.node_id < scene.nodes.size() &&
                instancer.logical_instance_count == instancer.proto_indices.size() &&
                instancer.positions.size() == instancer.proto_indices.size(),
                "PointInstancer has inconsistent required instance arrays.");
            const auto validate_optional_count = [&instancer](uint64_t count) {
                return count == 0 || count == instancer.logical_instance_count;
            };
            util::Logger::g_logger.assert_with_log(
                validate_optional_count(instancer.orientations.size()) &&
                validate_optional_count(instancer.scales.size()) &&
                validate_optional_count(instancer.velocities.size()) &&
                validate_optional_count(instancer.angular_velocities.size()) &&
                validate_optional_count(instancer.ids.size()),
                "PointInstancer has inconsistent optional instance arrays.");
            for (const auto proto_index : instancer.proto_indices) {
                util::Logger::g_logger.assert_with_log(
                    proto_index >= 0 &&
                    static_cast<size_t>(proto_index) < instancer.prototype_source_ids.size(),
                    "PointInstancer protoIndices references an invalid prototype.");
            }
        }

        for (const auto& asset : scene.source_assets) {
            validate_reference_(asset.source, "Source asset");
            util::Logger::g_logger.assert_with_log(
                !asset.authored_path.empty(),
                "Source asset does not preserve its authored path.");
        }
        for (const auto& graph : scene.material_graphs) {
            validate_reference_(graph.source, "Material graph");
            for (const auto& connection : graph.connections) {
                util::Logger::g_logger.assert_with_log(
                    !connection.source_property_path.empty() &&
                    !connection.destination_property_path.empty(),
                    "Material graph has an invalid connection.");
            }
        }
    }

} // namespace scene
