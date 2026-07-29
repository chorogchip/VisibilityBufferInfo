#include "scene/builder/cpu/JungleSceneCPUDrawStreamBuilder.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include <DirectXMath.h>

#include "util/Logger.h"

namespace scene {

    namespace {

        DirectX::XMMATRIX point_transform(
            const JungleSceneCPUData::PointInstance& instance) {

            return DirectX::XMMatrixMultiply(
                DirectX::XMMatrixScaling(
                    instance.scale.x,
                    instance.scale.y,
                    instance.scale.z),
                DirectX::XMMatrixMultiply(
                    DirectX::XMMatrixRotationQuaternion(
                        DirectX::XMLoadFloat4(
                            &instance.rotation)),
                    DirectX::XMMatrixTranslation(
                        instance.translation.x,
                        instance.translation.y,
                        instance.translation.z)));
        }

        bool is_visible(
            const JungleSceneCPUData& scene,
            const JungleSceneCPUData::PointPrototype& prototype,
            uint32_t point_instance_id,
            const DirectX::BoundingFrustum& frustum) {

            const JungleSceneCPUData::PointInstance& instance =
                scene.point_instances[point_instance_id];
            const DirectX::XMMATRIX world_transform =
                DirectX::XMMatrixMultiply(
                    DirectX::XMMatrixMultiply(
                        DirectX::XMLoadFloat4x4(
                            &prototype.prototype_local_transform),
                        point_transform(instance)),
                    DirectX::XMLoadFloat4x4(
                        &prototype.instancer_world_transform));
            const math::AABB& local_aabb =
                scene.scene.meshes[prototype.mesh_id].local_aabb;
            const DirectX::XMFLOAT3 center = {
                (local_aabb.pos_min.x + local_aabb.pos_max.x) * 0.5f,
                (local_aabb.pos_min.y + local_aabb.pos_max.y) * 0.5f,
                (local_aabb.pos_min.z + local_aabb.pos_max.z) * 0.5f
            };
            const float extent_x =
                (local_aabb.pos_max.x - local_aabb.pos_min.x) * 0.5f;
            const float extent_y =
                (local_aabb.pos_max.y - local_aabb.pos_min.y) * 0.5f;
            const float extent_z =
                (local_aabb.pos_max.z - local_aabb.pos_min.z) * 0.5f;
            const DirectX::BoundingSphere local_sphere(
                center,
                std::sqrt(
                    extent_x * extent_x +
                    extent_y * extent_y +
                    extent_z * extent_z));
            DirectX::BoundingSphere world_sphere{};
            local_sphere.Transform(
                world_sphere,
                world_transform);
            return frustum.Intersects(world_sphere);
        }

        void append_draws(
            const JungleSceneCPUData& scene,
            JungleSceneCPUDrawStream& stream,
            uint32_t prototype_id,
            uint32_t first_instance,
            uint32_t instance_count) {

            if (instance_count == 0) {
                return;
            }
            const JungleSceneCPUData::PointPrototype& prototype =
                scene.point_prototypes[prototype_id];
            const SceneCPUData::Mesh& mesh =
                scene.scene.meshes[prototype.mesh_id];
            const uint32_t submesh_end =
                mesh.first_submesh + mesh.submesh_count;
            for (uint32_t submesh_id = mesh.first_submesh;
                submesh_id < submesh_end;
                ++submesh_id) {
                const SceneCPUData::Submesh& submesh =
                    scene.scene.submeshes[submesh_id];
                stream.point_draw_calls_compacted.push_back({
                    first_instance,
                    instance_count,
                    prototype_id,
                    submesh_id,
                    submesh.index_count,
                    submesh.index_offset,
                    submesh.vertex_offset,
                    submesh.material_id
                });
            }
        }

        void validate(
            const JungleSceneCPUData& scene,
            const JungleSceneCPUDrawStream& stream) {

            util::Logger::g_logger.assert_with_log(
                stream.point_instance_ids_compacted.size() <=
                    (std::numeric_limits<uint32_t>::max)() &&
                stream.point_draw_calls_compacted.size() <=
                    (std::numeric_limits<uint32_t>::max)(),
                "Jungle compact point draw stream exceeds 32-bit indexing.");

            for (const uint32_t point_instance_id :
                stream.point_instance_ids_compacted) {
                util::Logger::g_logger.assert_with_log(
                    point_instance_id < scene.point_instances.size(),
                    "Jungle point draw stream references an invalid "
                    "compact instance.");
            }

            for (const auto& draw :
                stream.point_draw_calls_compacted) {
                const uint64_t instance_end =
                    static_cast<uint64_t>(draw.first_instance) +
                    draw.instance_count;
                util::Logger::g_logger.assert_with_log(
                    draw.instance_count > 0 &&
                    instance_end <=
                        stream.point_instance_ids_compacted.size() &&
                    draw.prototype_id <
                        scene.point_prototypes.size() &&
                    draw.submesh_id < scene.scene.submeshes.size(),
                    "Jungle point draw call has an invalid range.");

                const auto& prototype =
                    scene.point_prototypes[draw.prototype_id];
                const auto& mesh =
                    scene.scene.meshes[prototype.mesh_id];
                const auto& submesh =
                    scene.scene.submeshes[draw.submesh_id];
                util::Logger::g_logger.assert_with_log(
                    draw.submesh_id >= mesh.first_submesh &&
                    draw.submesh_id <
                        mesh.first_submesh + mesh.submesh_count &&
                    draw.index_count == submesh.index_count &&
                    draw.index_offset == submesh.index_offset &&
                    draw.vertex_offset == submesh.vertex_offset &&
                    draw.material_id == submesh.material_id,
                    "Jungle point draw call disagrees with prototype "
                    "geometry.");
            }
        }

    } // namespace

    void JungleSceneCPUDrawStreamBuilder::build_all(
        const JungleSceneCPUData& scene,
        JungleSceneCPUDrawStream& stream) {

        stream.point_instance_ids_compacted =
            scene.point_instance_ids_by_prototype;
        stream.point_draw_calls_compacted.clear();
        for (uint32_t prototype_id = 0;
            prototype_id < scene.point_prototypes.size();
            ++prototype_id) {
            const auto& prototype =
                scene.point_prototypes[prototype_id];
            append_draws(
                scene,
                stream,
                prototype_id,
                prototype.first_instance_id,
                prototype.instance_count);
        }
        validate(scene, stream);
    }

    void JungleSceneCPUDrawStreamBuilder::build_visible(
        const JungleSceneCPUData& scene,
        JungleSceneCPUDrawStream& stream,
        const DirectX::BoundingFrustum& frustum) {

        stream.point_instance_ids_compacted.clear();
        stream.point_draw_calls_compacted.clear();
        stream.point_instance_ids_compacted.reserve(
            scene.point_instance_ids_by_prototype.size());
        stream.point_draw_calls_compacted.reserve(
            scene.point_prototypes.size());

        for (uint32_t prototype_id = 0;
            prototype_id < scene.point_prototypes.size();
            ++prototype_id) {
            const auto& prototype =
                scene.point_prototypes[prototype_id];
            const uint32_t first_visible =
                static_cast<uint32_t>(
                    stream.point_instance_ids_compacted.size());
            uint32_t visible_count = 0;
            const uint32_t source_end =
                prototype.first_instance_id +
                prototype.instance_count;
            for (uint32_t source_id = prototype.first_instance_id;
                source_id < source_end;
                ++source_id) {
                const uint32_t point_instance_id =
                    scene.point_instance_ids_by_prototype[source_id];
                if (is_visible(
                        scene,
                        prototype,
                        point_instance_id,
                        frustum)) {
                    stream.point_instance_ids_compacted.push_back(
                        point_instance_id);
                    ++visible_count;
                }
            }
            append_draws(
                scene,
                stream,
                prototype_id,
                first_visible,
                visible_count);
        }
        validate(scene, stream);
    }

    uint64_t JungleSceneCPUDrawStreamBuilder::count_indices(
        const JungleSceneCPUDrawStream& stream) {

        uint64_t count = 0;
        for (const auto& draw :
            stream.point_draw_calls_compacted) {
            count += static_cast<uint64_t>(draw.index_count) *
                draw.instance_count;
        }
        return count;
    }

} // namespace scene
