#include "scene/builder/source/SceneRawJungleToSource.h"
#include "scene/raw/SceneRawJungle.h"

#include <cmath>
#include <iostream>

int main() {
    const auto raw = scene::raw::SceneRawJungle::open(SCENE_RAW_JUNGLE_SOURCE_FIXTURE);
    const auto source = scene::SceneRawJungleToSource::build(*raw);
    const auto source_again = scene::SceneRawJungleToSource::build(*raw);

    const bool has_native_instances =
        !source->native_prototypes.empty() &&
        !source->native_instances.empty() &&
        !source_again->native_prototypes.empty() &&
        !source_again->native_instances.empty();
    bool stable_source_ids = false;
    bool native_transform_is_preserved = false;
    if (has_native_instances) {
        const auto native_instance = source->native_instances.front();
        const auto& native_node = source->nodes[native_instance.node_id];
        stable_source_ids =
            source->nodes.size() == source_again->nodes.size() &&
            source->native_prototypes.front().source.stable_id ==
                source_again->native_prototypes.front().source.stable_id &&
            native_instance.source.stable_id ==
                source_again->native_instances.front().source.stable_id;
        native_transform_is_preserved =
            std::abs(native_node.world_transform._41 - 4.0f) < 0.0001f;
    }

    uint64_t shader_connection_count = 0;
    for (const auto& graph : source->material_graphs) {
        shader_connection_count += graph.connections.size();
    }

    const bool passed =
        source->nodes.size() >= 6 &&
        source->polygon_meshes.size() >= 1 &&
        source->polygon_meshes.front().face_vertex_counts.size() == 1 &&
        source->polygon_meshes.front().face_vertex_indices.size() == 4 &&
        !source->native_prototypes.empty() &&
        !source->native_instances.empty() &&
        source->point_instancers.size() == 1 &&
        source->point_instancers.front().logical_instance_count == 2 &&
        source->point_instancers.front().inactive_ids.size() == 1 &&
        source->point_instancers.front().inactive_ids.front() == 10 &&
        !source->material_graphs.empty() &&
        !source->shader_nodes.empty() &&
        shader_connection_count > 0 &&
        source->source_cameras.size() == 1 &&
        source->source_lights.size() == 1 &&
        native_transform_is_preserved &&
        stable_source_ids;
    if (!passed) {
        std::cerr << "SceneRawJungleToSource fixture validation failed.\n";
        return 1;
    }
    return 0;
}
