#include "scene/data/gpu/SceneGPUData.h"

#include <cstdint>

#include "util/Logger.h"

namespace scene {

    void SceneGPUData::validate() const {
        util::Logger::g_logger.assert_with_log(
            static_cast<bool>(vertex_buffer) &&
            static_cast<bool>(index_buffer) &&
            static_cast<bool>(mesh_buffer) &&
            static_cast<bool>(submesh_buffer) &&
            static_cast<bool>(material_buffer) &&
            static_cast<bool>(instance_buffer) &&
            static_cast<bool>(render_instance_buffer) &&
            static_cast<bool>(draw_buffer),
            "GPU scene has an unallocated global buffer.");
        util::Logger::g_logger.assert_with_log(
            vertex_count > 0 &&
            index_count > 0 &&
            mesh_count > 0 &&
            submesh_count > 0 &&
            material_count > 0 &&
            instance_count > 0 &&
            render_instance_count > 0 &&
            draw_count > 0 &&
            draw_calls.size() == draw_count,
            "GPU scene has invalid element counts.");
        util::Logger::g_logger.assert_with_log(
            vertex_buffer_view.BufferLocation ==
            vertex_buffer.get()->GetGPUVirtualAddress() &&
            vertex_buffer_view.SizeInBytes ==
            static_cast<uint64_t>(vertex_count) * sizeof(SceneCPUData::Vertex) &&
            vertex_buffer_view.StrideInBytes == sizeof(SceneCPUData::Vertex),
            "GPU scene vertex-buffer view is invalid.");
        util::Logger::g_logger.assert_with_log(
            index_buffer_view.BufferLocation ==
            index_buffer.get()->GetGPUVirtualAddress() &&
            index_buffer_view.SizeInBytes ==
            static_cast<uint64_t>(index_count) * sizeof(uint32_t) &&
            index_buffer_view.Format == DXGI_FORMAT_R32_UINT,
            "GPU scene index-buffer view is invalid.");

        uint64_t instance_cursor = 0;
        for (const SceneCPUData::DrawCall& draw : draw_calls) {
            const uint64_t index_end =
                static_cast<uint64_t>(draw.index_offset) +
                draw.index_count;
            util::Logger::g_logger.assert_with_log(
                draw.first_instance == instance_cursor &&
                draw.instance_count > 0 &&
                draw.submesh_id < submesh_count &&
                index_end <= index_count &&
                draw.vertex_offset < vertex_count &&
                draw.material_id < material_count,
                "GPU scene draw call is invalid.");
            instance_cursor += draw.instance_count;
        }
        util::Logger::g_logger.assert_with_log(
            instance_cursor == render_instance_count,
            "GPU scene draw calls do not cover the render-instance buffer.");

        for (const eng::GPUResource& texture : textures) {
            util::Logger::g_logger.assert_with_log(
                static_cast<bool>(texture),
                "GPU scene has an unallocated texture.");
        }
    }
}
