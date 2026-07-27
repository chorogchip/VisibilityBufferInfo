#include "scene/builder/cpu/SceneCPUDrawStreamBuilder.h"

#include <cstdint>
#include <limits>

#include "util/Logger.h"

namespace scene {

    namespace {

        bool is_visible(
            const SceneCPUData& scene,
            uint32_t draw_instance_id,
            const DirectX::BoundingFrustum& frustum) {
            const SceneCPUData::DrawInstance& draw_instance =
                scene.draw_instances[draw_instance_id];
            return frustum.Intersects(
                scene.instances[draw_instance.instance_id].
                    world_aabb.to_bounding_box());
        }

        void append_visible_draw(
            const SceneCPUData& scene,
            SceneCPUDrawStream& stream,
            const SceneCPUData::DrawCall& source,
            const DirectX::BoundingFrustum& frustum) {
            const uint32_t end =
                source.first_instance + source.instance_count;
            const uint32_t first_compacted =
                static_cast<uint32_t>(
                    stream.draw_instance_ids_compacted.size());
            uint32_t visible_count = 0;

            for (uint32_t draw_instance_id = source.first_instance;
                draw_instance_id < end;
                ++draw_instance_id) {
                if (is_visible(scene, draw_instance_id, frustum)) {
                    stream.draw_instance_ids_compacted.push_back(
                        draw_instance_id);
                    ++visible_count;
                }
            }

            if (visible_count > 0) {
                SceneCPUData::DrawCall draw = source;
                draw.first_instance = first_compacted;
                draw.instance_count = visible_count;
                stream.draw_calls_compacted.emplace_back(draw);
            }
        }

        void validate_stream(
            const SceneCPUData& scene,
            const SceneCPUDrawStream& stream) {
            util::Logger::g_logger.assert_with_log(
                stream.draw_instance_ids_compacted.size() <=
                (std::numeric_limits<uint32_t>::max)() &&
                stream.draw_calls_compacted.size() <=
                (std::numeric_limits<uint32_t>::max)(),
                "CPU draw stream exceeds 32-bit indexing.");

            for (uint32_t draw_instance_id :
                stream.draw_instance_ids_compacted) {
                util::Logger::g_logger.assert_with_log(
                    draw_instance_id < scene.draw_instances.size(),
                    "CPU draw stream references an invalid draw instance.");
            }

            uint64_t compacted_cursor = 0;
            for (const SceneCPUData::DrawCall& draw :
                stream.draw_calls_compacted) {
                const uint64_t compacted_end =
                    static_cast<uint64_t>(draw.first_instance) +
                    draw.instance_count;
                util::Logger::g_logger.assert_with_log(
                    draw.instance_count > 0 &&
                    draw.first_instance == compacted_cursor &&
                    compacted_end <=
                    stream.draw_instance_ids_compacted.size() &&
                    draw.submesh_id < scene.submeshes.size(),
                    "CPU draw stream has an invalid compacted draw call.");

                const SceneCPUData::Submesh& submesh =
                    scene.submeshes[draw.submesh_id];
                util::Logger::g_logger.assert_with_log(
                    draw.index_count == submesh.index_count &&
                    draw.index_offset == submesh.index_offset &&
                    draw.vertex_offset == submesh.vertex_offset &&
                    draw.material_id == submesh.material_id,
                    "CPU draw stream draw call disagrees with its submesh.");

                const uint32_t instance_end =
                    draw.first_instance + draw.instance_count;
                for (uint32_t cursor = draw.first_instance;
                    cursor < instance_end;
                    ++cursor) {
                    const SceneCPUData::DrawInstance& draw_instance =
                        scene.draw_instances[
                            stream.draw_instance_ids_compacted[cursor]];
                    util::Logger::g_logger.assert_with_log(
                        draw_instance.submesh_id == draw.submesh_id,
                        "CPU draw stream draw call contains a mismatched submesh.");
                }

                compacted_cursor = compacted_end;
            }
        }
    }

    void SceneCPUDrawStreamBuilder::build_all(
        const SceneCPUData& scene,
        SceneCPUDrawStream& stream) {
        stream.draw_instance_ids_compacted.clear();
        stream.draw_instance_ids_compacted.reserve(
            scene.draw_instances.size());
        for (uint32_t draw_instance_id = 0;
            draw_instance_id < scene.draw_instances.size();
            ++draw_instance_id) {
            stream.draw_instance_ids_compacted.push_back(draw_instance_id);
        }
        stream.draw_calls_compacted = scene.draw_calls;
        validate_stream(scene, stream);
    }

    void SceneCPUDrawStreamBuilder::build_visible(
        const SceneCPUData& scene,
        SceneCPUDrawStream& stream,
        const DirectX::BoundingFrustum& frustum) {
        util::Logger::g_logger.assert_with_log(
            !scene.draw_calls.empty(),
            "CPU scene has no complete draw stream.");

        stream.draw_instance_ids_compacted.clear();
        stream.draw_calls_compacted.clear();
        stream.draw_instance_ids_compacted.reserve(
            scene.draw_instances.size());
        stream.draw_calls_compacted.reserve(scene.draw_calls.size());
        for (const SceneCPUData::DrawCall& draw : scene.draw_calls) {
            append_visible_draw(scene, stream, draw, frustum);
        }
        validate_stream(scene, stream);
    }

    uint64_t SceneCPUDrawStreamBuilder::count_indices(
        const SceneCPUDrawStream& stream) {
        uint64_t count = 0;
        for (const SceneCPUData::DrawCall& draw :
            stream.draw_calls_compacted) {
            count += static_cast<uint64_t>(draw.index_count) *
                draw.instance_count;
        }
        return count;
    }
}
