#include "scene/builder/cpu/SceneCPUDrawBuilder.h"

#include <cstdint>

#include "util/Logger.h"

namespace scene {

    namespace {

        bool is_visible(
            const SceneCPUData& scene,
            uint32_t render_instance_id,
            const DirectX::BoundingFrustum& frustum) {
            const uint32_t instance_id =
                scene.draw_instance_ids[render_instance_id];
            return frustum.Intersects(
                scene.instances[instance_id].world_aabb.to_bounding_box());
        }

        void append_visible_runs(
            SceneCPUData& scene,
            const SceneCPUData::DrawCall& source,
            const DirectX::BoundingFrustum& frustum) {
            const uint32_t end =
                source.first_instance + source.instance_count;
            uint32_t cursor = source.first_instance;

            while (cursor < end) {
                while (cursor < end &&
                    !is_visible(scene, cursor, frustum)) {
                    ++cursor;
                }
                if (cursor == end) break;

                const uint32_t begin = cursor;
                while (cursor < end &&
                    is_visible(scene, cursor, frustum)) {
                    ++cursor;
                }

                SceneCPUData::DrawCall draw = source;
                draw.first_instance = begin;
                draw.instance_count = cursor - begin;
                scene.draw_calls.emplace_back(draw);
            }
        }
    }

    void SceneCPUDrawBuilder::reset(SceneCPUData& scene) {
        scene.draw_calls = scene.all_draw_calls;
    }

    void SceneCPUDrawBuilder::build_visible(
        SceneCPUData& scene,
        const DirectX::BoundingFrustum& frustum) {
        util::Logger::g_logger.assert_with_log(
            !scene.all_draw_calls.empty(),
            "CPU scene has no complete draw stream.");

        scene.draw_calls.clear();
        scene.draw_calls.reserve(scene.all_draw_calls.size());
        for (const SceneCPUData::DrawCall& draw : scene.all_draw_calls) {
            append_visible_runs(scene, draw, frustum);
        }
    }

    uint64_t SceneCPUDrawBuilder::count_indices(
        const SceneCPUData& scene) {
        uint64_t count = 0;
        for (const SceneCPUData::DrawCall& draw : scene.draw_calls) {
            count += static_cast<uint64_t>(draw.index_count) *
                draw.instance_count;
        }
        return count;
    }
}
