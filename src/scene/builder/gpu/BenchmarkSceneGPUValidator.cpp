#include "scene/builder/gpu/BenchmarkSceneGPUValidator.h"

#include <cstdint>

#include "util/Logger.h"

namespace scene {

    void BenchmarkSceneGPUValidator::validate(
        const BenchmarkSceneGPUData& scene) {

        util::Logger::g_logger.assert_with_log(
            static_cast<bool>(scene.vertex_buffer) &&
            static_cast<bool>(scene.index_buffer) &&
            static_cast<bool>(scene.mesh_buffer) &&
            static_cast<bool>(scene.submesh_buffer) &&
            static_cast<bool>(scene.material_buffer) &&
            static_cast<bool>(scene.instance_buffer) &&
            static_cast<bool>(scene.render_instance_buffer) &&
            static_cast<bool>(scene.draw_buffer),
            "Benchmark GPU scene has an unallocated global buffer.");
        util::Logger::g_logger.assert_with_log(
            scene.vertex_count > 0 && scene.index_count > 0,
            "Benchmark GPU scene has no geometry.");
        util::Logger::g_logger.assert_with_log(
            scene.mesh_count > 0 && scene.submesh_count > 0,
            "Benchmark GPU scene has no mesh data.");
        util::Logger::g_logger.assert_with_log(
            scene.material_count > 0,
            "Benchmark GPU scene has no materials.");
        util::Logger::g_logger.assert_with_log(
            scene.instance_count > 0 &&
            scene.render_instance_count > 0,
            "Benchmark GPU scene has no instances.");
        util::Logger::g_logger.assert_with_log(
            scene.draw_count > 0 &&
            scene.draw_calls.size() == scene.draw_count,
            "Benchmark GPU scene has invalid draw counts.");
        util::Logger::g_logger.assert_with_log(
            scene.vertex_buffer_view.BufferLocation ==
            scene.vertex_buffer.get()->GetGPUVirtualAddress() &&
            scene.vertex_buffer_view.SizeInBytes ==
            static_cast<uint64_t>(scene.vertex_count) *
            sizeof(SceneCPUData::Vertex) &&
            scene.vertex_buffer_view.StrideInBytes ==
            sizeof(SceneCPUData::Vertex),
            "Benchmark GPU scene vertex-buffer view is invalid.");
        util::Logger::g_logger.assert_with_log(
            scene.index_buffer_view.BufferLocation ==
            scene.index_buffer.get()->GetGPUVirtualAddress() &&
            scene.index_buffer_view.SizeInBytes ==
            static_cast<uint64_t>(scene.index_count) *
            sizeof(uint32_t) &&
            scene.index_buffer_view.Format == DXGI_FORMAT_R32_UINT,
            "Benchmark GPU scene index-buffer view is invalid.");

        uint64_t instance_cursor = 0;
        for (const SceneCPUData::DrawCall& draw : scene.draw_calls) {
            const uint64_t index_end =
                static_cast<uint64_t>(draw.index_offset) +
                draw.index_count;
            util::Logger::g_logger.assert_with_log(
                draw.first_instance == instance_cursor &&
                draw.instance_count > 0 &&
                draw.submesh_id < scene.submesh_count &&
                index_end <= scene.index_count &&
                draw.vertex_offset < scene.vertex_count &&
                draw.material_id < scene.material_count,
                "Benchmark GPU scene draw call is invalid.");
            instance_cursor += draw.instance_count;
        }
        util::Logger::g_logger.assert_with_log(
            instance_cursor == scene.render_instance_count,
            "Benchmark GPU scene draws do not cover render instances.");

        for (const eng::GPUResource& texture : scene.textures) {
            util::Logger::g_logger.assert_with_log(
                static_cast<bool>(texture),
                "Benchmark GPU scene has an unallocated texture.");
        }
    }
}
