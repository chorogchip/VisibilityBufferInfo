#pragma once

#include <cstdint>
#include <vector>

#include "scene/data/cpu/SceneCPUData.h"
#include "scene/data/source/SceneSourceData.h"

namespace scene {

    struct JungleSceneMaterialization {
        // Explicit triangle-only representation consumed by SceneCPUBuilder.
        // USD polygon topology, graphs, and prototype relations remain in the
        // separate semantic SceneSourceData passed to materialize().
        SceneSourceData legacy_scene;
        std::vector<uint32_t> semantic_node_to_legacy_node;
        uint64_t expanded_point_instance_count = 0;
        uint64_t native_instance_count = 0;
        uint64_t materialized_instance_count = 0;
        uint64_t materialized_draw_instance_count = 0;
        uint32_t shared_prototype_mesh_count = 0;
    };

    class JungleSceneCPUBuilder {
    public:
        static JungleSceneMaterialization materialize(
            SceneSourceData& semantic_scene);

        static SceneCPUData build(SceneSourceData& semantic_scene);
    };

} // namespace scene
