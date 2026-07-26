#include "scene/builder/gpu/DonutSceneGPUValidator.h"

#include <cstdint>

#include "util/Logger.h"

namespace scene {

    void DonutSceneGPUValidator::validate(
        const DonutSceneGPUData& scene) {

        util::Logger::g_logger.assert_with_log(
            static_cast<bool>(scene.vertex_buffer) &&
            static_cast<bool>(scene.index_buffer) &&
            static_cast<bool>(scene.instance_buffer) &&
            static_cast<bool>(scene.render_instance_buffer) &&
            static_cast<bool>(scene.submesh_buffer) &&
            static_cast<bool>(scene.geometry_instance_buffer) &&
            static_cast<bool>(scene.material_buffer) &&
            static_cast<bool>(scene.material_constant_buffer),
            "Donut GPU scene has an unallocated global buffer.");
        util::Logger::g_logger.assert_with_log(
            scene.vertex_count > 0 &&
            scene.index_count > 0 &&
            !scene.instance_data.empty() &&
            !scene.render_instance_data.empty() &&
            !scene.submesh_data.empty() &&
            !scene.geometry_instance_data.empty() &&
            !scene.material_data.empty() &&
            !scene.draws.empty(),
            "Donut GPU scene has missing render data.");
        util::Logger::g_logger.assert_with_log(
            scene.vertex_layout.position_offset <
            scene.vertex_layout.byte_size &&
            scene.vertex_layout.prev_position_offset <
            scene.vertex_layout.byte_size &&
            scene.vertex_layout.texcoord_offset <
            scene.vertex_layout.byte_size &&
            scene.vertex_layout.normal_offset <
            scene.vertex_layout.byte_size &&
            scene.vertex_layout.tangent_offset <
            scene.vertex_layout.byte_size,
            "Donut GPU scene has an invalid vertex layout.");
        util::Logger::g_logger.assert_with_log(
            scene.index_buffer_view.BufferLocation ==
            scene.index_buffer.get()->GetGPUVirtualAddress() &&
            scene.index_buffer_view.SizeInBytes ==
            static_cast<uint64_t>(scene.index_count) *
            sizeof(uint32_t) &&
            scene.index_buffer_view.Format == DXGI_FORMAT_R32_UINT,
            "Donut GPU scene has an invalid index-buffer view.");
        util::Logger::g_logger.assert_with_log(
            scene.render_instance_data.size() <=
            scene.render_instance_capacity,
            "Donut GPU scene exceeds its render-instance capacity.");

        for (const DonutSceneGPUData::InstanceData& instance : scene.instance_data) {
            const uint64_t geometry_end =
                static_cast<uint64_t>(
                    instance.first_geometry_instance) +
                instance.geometry_instance_count;
            util::Logger::g_logger.assert_with_log(
                instance.geometry_instance_count > 0 &&
                geometry_end <=
                scene.geometry_instance_data.size(),
                "Donut GPU scene instance has an invalid geometry range.");
        }

        for (const DonutSceneGPUData::GeometryInstanceData& geometry : scene.geometry_instance_data) {
            util::Logger::g_logger.assert_with_log(
                geometry.instance_id < scene.instance_data.size() &&
                geometry.submesh_id < scene.submesh_data.size(),
                "Donut GPU scene has an invalid geometry instance.");
        }

        uint64_t render_instance_cursor = 0;
        for (const DonutSceneGPUData::DrawData& draw : scene.draws) {
            util::Logger::g_logger.assert_with_log(
                draw.first_render_instance ==
                render_instance_cursor &&
                draw.instance_count > 0 &&
                draw.submesh_id < scene.submesh_data.size() &&
                draw.material_id < scene.material_data.size(),
                "Donut GPU scene has an invalid draw.");
            render_instance_cursor += draw.instance_count;
        }
        util::Logger::g_logger.assert_with_log(
            render_instance_cursor ==
            scene.render_instance_data.size(),
            "Donut GPU scene draws do not cover render instances.");

        for (const DonutSceneGPUData::MaterialData& material : scene.material_data) {
            for (uint32_t texture_id : material.texture_indices) {
                util::Logger::g_logger.assert_with_log(
                    texture_id < scene.textures.size(),
                    "Donut GPU scene material references an invalid texture.");
            }
        }
        for (const eng::GPUResource& texture : scene.textures) {
            util::Logger::g_logger.assert_with_log(
                static_cast<bool>(texture),
                "Donut GPU scene has an unallocated texture.");
        }
    }
}
