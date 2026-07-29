#pragma once

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "scene/data/cpu/JungleSceneCPUData.h"
#include "scene/data/cpu/SceneCPUData.h"
#include "scene/data/source/SceneSourceData.h"

namespace scene {

    struct JungleSceneMaterializationOptions {
        bool expand_point_instancers = true;
    };

    struct JunglePointPrototypeMaterialization {
        uint32_t point_instancer_id =
            source::SceneConstants::INVALID_INDEX;
        uint32_t prototype_index =
            source::SceneConstants::INVALID_INDEX;
        uint32_t mesh_id = source::SceneConstants::INVALID_INDEX;
        DirectX::XMFLOAT4X4 prototype_local_transform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    };

    struct JungleSceneMaterialization {
        // Explicit triangle-only representation consumed by SceneCPUBuilder.
        // USD polygon topology, graphs, and prototype relations remain in the
        // separate semantic SceneSourceData passed to materialize().
        SceneSourceData legacy_scene;
        std::vector<uint32_t> semantic_node_to_legacy_node;
        std::vector<JunglePointPrototypeMaterialization>
            point_prototypes;
        uint64_t logical_point_instance_count = 0;
        uint64_t expanded_point_instance_count = 0;
        uint64_t native_instance_count = 0;
        uint64_t materialized_instance_count = 0;
        uint64_t materialized_draw_instance_count = 0;
        uint32_t shared_prototype_mesh_count = 0;
    };

    class JungleSceneCPUBuilder {
    public:
        static JungleSceneMaterialization materialize(
            SceneSourceData& semantic_scene,
            const JungleSceneMaterializationOptions& options = {});

        static SceneCPUData build(SceneSourceData& semantic_scene);

        static JungleSceneCPUData build_compact(
            SceneSourceData& semantic_scene);
    };

} // namespace scene
