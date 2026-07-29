#include "scene/builder/cpu/SceneCPUBuilder.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <DirectXMath.h>

#include "scene/builder/cpu/SceneCPUValidator.h"
#include "scene/builder/source/SceneSourceDataValidator.h"
#include "util/Logger.h"

namespace scene {

    namespace {

        SceneCPUData::Material::TexturePath texture_path(
            const SceneSourceData& source,
            const source::TextureRef& reference) {
            if (!reference.valid()) {
                return std::nullopt;
            }
            const source::Texture& texture =
                source.textures[reference.texture_id];
            const source::Image& image =
                source.images[texture.image_id];
            if (image.is_file_range()) {
                return std::nullopt;
            }
            util::Logger::g_logger.assert_with_log(
                reference.uv_set == 0,
                "CPU scene supports only texture coordinate set 0.");
            return image.path;
        }

        void append_materials(
            const SceneSourceData& source,
            SceneCPUData& destination) {
            destination.materials.reserve(
                source.materials.empty() ? 1 : source.materials.size());

            for (const source::Material& material : source.materials) {
                SceneCPUData::Material converted{};
                converted.base_color = material.base_color;
                converted.emissive_color = material.emissive_color;
                converted.emissive_intensity = material.emissive_intensity;
                converted.metalness = material.metalness;
                converted.roughness = material.roughness;
                converted.opacity = material.base_color.w;
                converted.alpha_cutoff = material.alpha_cutoff;
                converted.normal_scale = material.normal_scale;
                converted.occlusion_strength = material.occlusion_strength;
                converted.alpha_mode = material.alpha_mode;
                converted.double_sided = material.double_sided;
                converted.virtual_shader_id = material.virtual_shader_id;
                converted.base_color_texture =
                    texture_path(source, material.base_color_texture);
                converted.metal_roughness_texture =
                    texture_path(
                        source,
                        material.metal_roughness_texture);
                converted.normal_texture =
                    texture_path(source, material.normal_texture);
                converted.emissive_texture =
                    texture_path(source, material.emissive_texture);
                converted.occlusion_texture =
                    texture_path(source, material.occlusion_texture);
                destination.materials.emplace_back(std::move(converted));
            }

            if (destination.materials.empty()) {
                destination.materials.emplace_back();
            }
        }

        void append_geometry(
            const SceneSourceData& source,
            SceneCPUData& destination) {
            destination.meshes.reserve(source.meshes.size());

            for (const source::Mesh& source_mesh : source.meshes) {
                util::Logger::g_logger.assert_with_log(
                    destination.submeshes.size() <=
                    (std::numeric_limits<uint32_t>::max)(),
                    "CPU scene submesh count exceeds 32-bit indexing.");

                SceneCPUData::Mesh mesh{};
                mesh.first_submesh =
                    static_cast<uint32_t>(destination.submeshes.size());
                mesh.local_aabb = math::AABB::create_empty();

                for (const source::Primitive& primitive : source_mesh.primitives) {
                    const size_t max_count =
                        (std::numeric_limits<uint32_t>::max)();
                    util::Logger::g_logger.assert_with_log(
                        destination.vertices.size() <= max_count &&
                        destination.indices.size() <= max_count &&
                        primitive.positions.size() <=
                        max_count - destination.vertices.size() &&
                        primitive.indices.size() <=
                        max_count - destination.indices.size(),
                        "CPU scene geometry exceeds 32-bit indexing.");

                    SceneCPUData::Submesh submesh{};
                    submesh.vertex_offset =
                        static_cast<uint32_t>(destination.vertices.size());
                    submesh.vertex_count =
                        static_cast<uint32_t>(primitive.positions.size());
                    submesh.index_offset =
                        static_cast<uint32_t>(destination.indices.size());
                    submesh.index_count =
                        static_cast<uint32_t>(primitive.indices.size());
                    submesh.material_id =
                        primitive.material_id ==
                        source::SceneConstants::INVALID_INDEX
                        ? 0
                        : primitive.material_id;
                    submesh.local_aabb = math::AABB::create_empty();

                    for (size_t i = 0; i < primitive.positions.size(); ++i) {
                        SceneCPUData::Vertex vertex{};
                        vertex.position = primitive.positions[i];
                        vertex.normal = primitive.normals.empty()
                            ? DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f }
                            : primitive.normals[i];
                        vertex.tangent = primitive.tangents.empty()
                            ? DirectX::XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f }
                            : primitive.tangents[i];
                        vertex.uv0 = primitive.uv0.empty()
                            ? DirectX::XMFLOAT2{ 0.0f, 0.0f }
                            : primitive.uv0[i];
                        destination.vertices.emplace_back(vertex);
                        submesh.local_aabb = submesh.local_aabb.get_union(
                            math::AABB::create_from_pos(vertex.position));
                    }

                    destination.indices.insert(
                        destination.indices.end(),
                        primitive.indices.begin(),
                        primitive.indices.end());
                    mesh.local_aabb =
                        mesh.local_aabb.get_union(submesh.local_aabb);
                    destination.submeshes.emplace_back(submesh);
                }

                mesh.submesh_count =
                    static_cast<uint32_t>(destination.submeshes.size()) -
                    mesh.first_submesh;
                destination.meshes.emplace_back(mesh);
            }
        }

        void append_instance(
            uint32_t mesh_id,
            DirectX::FXMMATRIX world_transform,
            SceneCPUData& destination) {
            util::Logger::g_logger.assert_with_log(
                destination.instances.size() <
                (std::numeric_limits<uint32_t>::max)(),
                "CPU scene instance count exceeds 32-bit indexing.");

            SceneCPUData::Instance instance{};
            instance.mesh_id = mesh_id;
            DirectX::XMStoreFloat4x4(
                &instance.world_transform,
                world_transform);
            instance.world_aabb =
                destination.meshes[mesh_id].local_aabb.get_transformed(
                    instance.world_transform);
            destination.world_aabb =
                destination.world_aabb.get_union(instance.world_aabb);
            destination.instances.emplace_back(instance);
        }

        math::AABB append_node(
            const SceneSourceData& source,
            uint32_t node_id,
            DirectX::FXMMATRIX parent_world,
            SceneCPUData& destination) {
            const source::Node& node = source.nodes[node_id];
            const DirectX::XMMATRIX local =
                DirectX::XMLoadFloat4x4(&node.local_transform);
            const DirectX::XMMATRIX node_world =
                DirectX::XMMatrixMultiply(local, parent_world);

            SceneCPUData::Node& converted = destination.nodes[node_id];
            converted.children = node.children;
            converted.local_transform = node.local_transform;
            converted.mesh_id = node.mesh_id;
            DirectX::XMStoreFloat4x4(
                &converted.world_transform,
                node_world);
            converted.first_instance =
                static_cast<uint32_t>(destination.instances.size());

            if (node.mesh_id != source::SceneConstants::INVALID_INDEX) {
                if (node.instance_count == 0) {
                    append_instance(
                        node.mesh_id,
                        node_world,
                        destination);
                } else {
                    const uint32_t instance_end =
                        node.first_instance + node.instance_count;
                    for (uint32_t instance_id = node.first_instance; instance_id < instance_end; ++instance_id) {
                        const source::InstanceTransform& source_instance =
                            source.instances[instance_id];
                        const DirectX::XMMATRIX instance_transform =
                            source_instance.has_matrix
                            ? DirectX::XMLoadFloat4x4(
                                &source_instance.matrix)
                            : DirectX::XMMatrixMultiply(
                                DirectX::XMMatrixScaling(
                                    source_instance.scale.x,
                                    source_instance.scale.y,
                                    source_instance.scale.z),
                                DirectX::XMMatrixMultiply(
                                    DirectX::XMMatrixRotationQuaternion(
                                        DirectX::XMLoadFloat4(
                                            &source_instance.rotation)),
                                    DirectX::XMMatrixTranslation(
                                        source_instance.translation.x,
                                        source_instance.translation.y,
                                        source_instance.translation.z)));
                        append_instance(
                            node.mesh_id,
                            DirectX::XMMatrixMultiply(
                                instance_transform,
                                node_world),
                            destination);
                    }
                }
            }
            converted.instance_count =
                static_cast<uint32_t>(destination.instances.size()) -
                converted.first_instance;

            math::AABB subtree = math::AABB::create_empty();
            for (uint32_t instance_id = converted.first_instance; instance_id < converted.first_instance + converted.instance_count; ++instance_id) {
                subtree = subtree.get_union(
                    destination.instances[instance_id].world_aabb);
            }
            for (uint32_t child_id : node.children) {
                subtree = subtree.get_union(
                    append_node(
                        source,
                        child_id,
                        node_world,
                        destination));
            }
            converted.subtree_world_aabb = subtree;
            return subtree;
        }

        void append_draw_calls(SceneCPUData& destination) {
            for (uint32_t instance_id = 0; instance_id < destination.instances.size(); ++instance_id) {
                const SceneCPUData::Instance& instance =
                    destination.instances[instance_id];
                const SceneCPUData::Mesh& mesh =
                    destination.meshes[instance.mesh_id];
                const uint32_t submesh_end =
                    mesh.first_submesh + mesh.submesh_count;
                for (uint32_t submesh_id = mesh.first_submesh; submesh_id < submesh_end; ++submesh_id) {
                    destination.draw_instances.push_back(
                        { instance_id, submesh_id });
                }
            }

            std::sort(
                destination.draw_instances.begin(),
                destination.draw_instances.end(),
                [](const SceneCPUData::DrawInstance& left,
                    const SceneCPUData::DrawInstance& right) {
                    if (left.submesh_id != right.submesh_id) {
                        return left.submesh_id < right.submesh_id;
                    }
                    return left.instance_id < right.instance_id;
                });

            size_t begin = 0;
            while (begin < destination.draw_instances.size()) {
                size_t end = begin + 1;
                while (end < destination.draw_instances.size() &&
                    destination.draw_instances[end].submesh_id ==
                    destination.draw_instances[begin].submesh_id) {
                    ++end;
                }

                util::Logger::g_logger.assert_with_log(
                    destination.draw_instances.size() <=
                    (std::numeric_limits<uint32_t>::max)() &&
                    destination.draw_calls.size() <
                    (std::numeric_limits<uint32_t>::max)(),
                    "CPU scene draw stream exceeds 32-bit indexing.");

                const uint32_t submesh_id =
                    destination.draw_instances[begin].submesh_id;
                const SceneCPUData::Submesh& submesh =
                    destination.submeshes[submesh_id];
                SceneCPUData::DrawCall draw{};
                draw.first_instance = static_cast<uint32_t>(begin);
                draw.instance_count =
                    static_cast<uint32_t>(end - begin);
                draw.submesh_id = submesh_id;
                draw.index_count = submesh.index_count;
                draw.index_offset = submesh.index_offset;
                draw.vertex_offset = submesh.vertex_offset;
                draw.material_id = submesh.material_id;
                destination.draw_calls.emplace_back(draw);

                begin = end;
            }
        }
    }

    SceneCPUData SceneCPUBuilder::build(const SceneSourceData& source) {
        SceneSourceDataValidator::validate(source);

        SceneCPUData destination{};
        append_materials(source, destination);
        append_geometry(source, destination);
        destination.root_node_id = source.root_node_id;
        destination.nodes.resize(source.nodes.size());
        destination.world_aabb = append_node(
            source,
            source.root_node_id,
            DirectX::XMMatrixIdentity(),
            destination);
        append_draw_calls(destination);
        destination.active_material_class_count = source.active_material_class_count;
        SceneCPUValidator::validate(destination);
        return destination;
    }
}
