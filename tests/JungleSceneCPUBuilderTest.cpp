#include "scene/builder/cpu/JungleSceneCPUBuilder.h"
#include "scene/builder/cpu/JungleSceneCPUDrawStreamBuilder.h"
#include "scene/builder/cpu/SceneCPUBuilder.h"
#include "scene/builder/source/JungleSceneSourceBuilder.h"

#include <iostream>
#include <string>

int main() {
    auto semantic = scene::JungleSceneSourceBuilder::build(
        JUNGLE_SCENE_CPU_SOURCE_FIXTURE);
    const size_t material_graph_count =
        semantic->material_graphs.size();
    const size_t point_instancer_count =
        semantic->point_instancers.size();

    scene::JungleSceneCPUData compact =
        scene::JungleSceneCPUBuilder::build_compact(*semantic);
    scene::JungleSceneCPUDrawStream compact_draw_stream{};
    scene::JungleSceneCPUDrawStreamBuilder::build_all(
        compact,
        compact_draw_stream);

    scene::JungleSceneMaterialization materialized =
        scene::JungleSceneCPUBuilder::materialize(*semantic);
    const scene::SceneCPUData cpu =
        scene::SceneCPUBuilder::build(
            materialized.legacy_scene);

    bool has_pbr_subset_diagnostic = false;
    bool has_prototype_compaction_diagnostic = false;
    bool has_prototype_alias_diagnostic = false;
    bool has_point_transform_diagnostic = false;
    for (const auto& diagnostic :
        semantic->conversion_diagnostics) {
        if (diagnostic.code == "legacy_pbr_subset") {
            has_pbr_subset_diagnostic = true;
        }
        if (diagnostic.code ==
            "prototype_submesh_compaction") {
            has_prototype_compaction_diagnostic = true;
        }
        if (diagnostic.code == "prototype_mesh_alias") {
            has_prototype_alias_diagnostic = true;
        }
        if (diagnostic.code ==
            "point_prototype_matrix_materialized") {
            has_point_transform_diagnostic = true;
        }
    }

    const auto& native_instance_a =
        semantic->native_instances[0];
    const auto& native_instance_b =
        semantic->native_instances[1];
    const uint32_t native_legacy_node_a =
        materialized.semantic_node_to_legacy_node[
            native_instance_a.node_id];
    const uint32_t native_legacy_node_b =
        materialized.semantic_node_to_legacy_node[
            native_instance_b.node_id];
    const uint32_t shared_mesh_id =
        materialized.legacy_scene.nodes[
            native_legacy_node_a].mesh_id;
    const bool native_instances_share_mesh =
        shared_mesh_id ==
        materialized.legacy_scene.nodes[
            native_legacy_node_b].mesh_id;

    const uint32_t point_host =
        materialized.semantic_node_to_legacy_node[
            semantic->point_instancers.front().node_id];
    uint32_t point_instances_materialized = 0;
    bool has_affine_point_instance = false;
    for (const uint32_t child :
        materialized.legacy_scene.nodes[
            point_host].children) {
        const auto& node =
            materialized.legacy_scene.nodes[child];
        if (node.name.starts_with(
                "PointInstancer prototype ") &&
            node.mesh_id !=
                scene::source::SceneConstants::INVALID_INDEX &&
            node.instance_count > 0) {
            point_instances_materialized +=
                node.instance_count;
            for (uint32_t instance_id = node.first_instance;
                instance_id <
                    node.first_instance + node.instance_count;
                ++instance_id) {
                has_affine_point_instance =
                    has_affine_point_instance ||
                    materialized.legacy_scene.instances[
                        instance_id].has_matrix;
            }
        }
    }

    bool compact_has_affine_prototype = false;
    for (const auto& prototype : compact.point_prototypes) {
        const DirectX::XMFLOAT4X4& matrix =
            prototype.prototype_local_transform;
        compact_has_affine_prototype =
            compact_has_affine_prototype ||
            matrix._11 != 1.0f ||
            matrix._22 != 1.0f ||
            matrix._33 != 1.0f ||
            matrix._41 != 0.0f ||
            matrix._42 != 0.0f ||
            matrix._43 != 0.0f;
    }

    bool has_triangulated_quad = false;
    bool has_vertex_attributes = false;
    for (const auto& mesh :
        materialized.legacy_scene.meshes) {
        for (const auto& primitive : mesh.primitives) {
            if (primitive.indices.size() == 6) {
                has_triangulated_quad = true;
            }
            if (primitive.normals.size() ==
                    primitive.positions.size() &&
                primitive.uv0.size() ==
                    primitive.positions.size()) {
                has_vertex_attributes = true;
            }
        }
    }

    const bool passed =
        semantic->material_graphs.size() ==
            material_graph_count &&
        semantic->point_instancers.size() ==
            point_instancer_count &&
        semantic->point_instancers.front().
            logical_instance_count == 2 &&
        materialized.expanded_point_instance_count == 2 &&
        materialized.legacy_scene.instances.size() == 2 &&
        compact.logical_point_instance_count == 2 &&
        compact.point_instances.size() == 2 &&
        compact.point_instance_ids_by_prototype.size() == 2 &&
        compact.point_prototypes.size() == 2 &&
        compact.scene.instances.size() == 4 &&
        compact.scene.draw_instances.size() == 4 &&
        compact_draw_stream.point_instance_ids_compacted.size() == 2 &&
        compact_draw_stream.point_draw_calls_compacted.size() == 2 &&
        scene::JungleSceneCPUDrawStreamBuilder::count_indices(
            compact_draw_stream) > 0 &&
        compact_has_affine_prototype &&
        materialized.native_instance_count == 4 &&
        materialized.materialized_instance_count == 6 &&
        materialized.materialized_draw_instance_count == 6 &&
        materialized.shared_prototype_mesh_count == 1 &&
        shared_mesh_id !=
            scene::source::SceneConstants::INVALID_INDEX &&
        native_instances_share_mesh &&
        point_instances_materialized == 2 &&
        has_affine_point_instance &&
        has_triangulated_quad &&
        has_vertex_attributes &&
        has_pbr_subset_diagnostic &&
        has_prototype_compaction_diagnostic &&
        has_prototype_alias_diagnostic &&
        has_point_transform_diagnostic &&
        !cpu.vertices.empty() &&
        !cpu.indices.empty() &&
        cpu.instances.size() >= 3;
    if (!passed) {
        std::cerr <<
            "Jungle source-to-CPU materialization fixture "
            "validation failed.\n"
            "material_graphs=" <<
            semantic->material_graphs.size() <<
            ", point_instancers=" <<
            semantic->point_instancers.size() <<
            ", expanded=" <<
            materialized.expanded_point_instance_count <<
            ", legacy_instances=" <<
            materialized.legacy_scene.instances.size() <<
            ", compact_instances=" <<
            compact.scene.instances.size() <<
            ", compact_point_instances=" <<
            compact.point_instances.size() <<
            ", compact_point_ids=" <<
            compact.point_instance_ids_by_prototype.size() <<
            ", compact_prototypes=" <<
            compact.point_prototypes.size() <<
            ", compact_draws=" <<
            compact_draw_stream.point_draw_calls_compacted.size() <<
            ", compact_affine=" <<
            compact_has_affine_prototype <<
            ", native_instances=" <<
            materialized.native_instance_count <<
            ", materialized_instances=" <<
            materialized.materialized_instance_count <<
            ", draw_instances=" <<
            materialized.materialized_draw_instance_count <<
            ", shared_prototype_meshes=" <<
            materialized.shared_prototype_mesh_count <<
            ", shared_mesh_id=" << shared_mesh_id <<
            ", native_share=" <<
            native_instances_share_mesh <<
            ", point_materialized=" <<
            point_instances_materialized <<
            ", affine_point_instance=" <<
            has_affine_point_instance <<
            ", triangulated_quad=" <<
            has_triangulated_quad <<
            ", vertex_attributes=" <<
            has_vertex_attributes <<
            ", pbr_diagnostic=" <<
            has_pbr_subset_diagnostic <<
            ", compaction_diagnostic=" <<
            has_prototype_compaction_diagnostic <<
            ", alias_diagnostic=" <<
            has_prototype_alias_diagnostic <<
            ", point_transform_diagnostic=" <<
            has_point_transform_diagnostic <<
            ", cpu_vertices=" << cpu.vertices.size() <<
            ", cpu_indices=" << cpu.indices.size() <<
            ", cpu_instances=" << cpu.instances.size() <<
            '\n';
        return 1;
    }
    return 0;
}
