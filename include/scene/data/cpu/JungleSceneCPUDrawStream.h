#pragma once

#include <cstdint>
#include <vector>

namespace scene {

    struct JungleSceneCPUDrawStream {
        struct PointDrawCall {
            uint32_t first_instance = 0;
            uint32_t instance_count = 0;
            uint32_t prototype_id = 0;
            uint32_t submesh_id = 0;
            uint32_t index_count = 0;
            uint32_t index_offset = 0;
            uint32_t vertex_offset = 0;
            uint32_t material_id = 0;
        };

        // A prototype's visible IDs are stored once and reused by all of its
        // submesh draw calls.
        std::vector<uint32_t> point_instance_ids_compacted;
        std::vector<PointDrawCall> point_draw_calls_compacted;
    };

    static_assert(
        sizeof(JungleSceneCPUDrawStream::PointDrawCall) == 32);

} // namespace scene
