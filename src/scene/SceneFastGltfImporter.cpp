#include "scene/SceneFastGltfImporter.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "scene/donut/DonutSceneFastGltfImporter.h"

namespace scene {

    namespace {

        float unpack_snorm8_component(uint32_t packed, uint32_t shift) {
            const int8_t value =
                static_cast<int8_t>((packed >> shift) & 0xffu);
            if (value == -128) return -1.0f;
            return std::clamp(static_cast<float>(value) / 127.0f, -1.0f, 1.0f);
        }

        DirectX::XMFLOAT3 unpack_snorm8_xyz(uint32_t packed) {
            return {
                unpack_snorm8_component(packed, 0),
                unpack_snorm8_component(packed, 8),
                unpack_snorm8_component(packed, 16)
            };
        }

        eng::MaterialCPU convert_material(
            const DonutSceneDataCPU& source,
            const DonutSceneDataCPU::Material& material) {

            eng::MaterialCPU result{};
            result.name = material.name;
            result.base_color = material.base_color;
            result.emissive_color = material.emissive_color;
            result.metalness = material.metalness;
            result.roughness = material.roughness;
            result.opacity = material.base_color.w;
            result.alpha_cutoff = material.alpha_cutoff;
            result.normal_scale = material.normal_scale;
            result.occlusion_strength = material.occlusion_strength;

            const auto set_texture =
                [&source, &material](DonutSceneDataCPU::EnumMaterialTextureSlot slot)
                -> eng::MaterialCPU::TexturePath {
                const uint32_t texture_id =
                    material.texture_ids[static_cast<size_t>(slot)];
                if (texture_id == DonutSceneDataCPU::INVALID_INDEX ||
                    texture_id >= source.textures.size()) {
                    return std::nullopt;
                }
                return source.textures[texture_id].path;
            };

            result.base_color_texture = set_texture(
                DonutSceneDataCPU::EnumMaterialTextureSlot::BASE_COLOR);
            result.normal_texture = set_texture(
                DonutSceneDataCPU::EnumMaterialTextureSlot::NORMAL);
            result.metal_roughness_texture = set_texture(
                DonutSceneDataCPU::EnumMaterialTextureSlot::METAL_ROUGHNESS);
            result.emissive_texture = set_texture(
                DonutSceneDataCPU::EnumMaterialTextureSlot::EMISSIVE);
            result.occlusion_texture = set_texture(
                DonutSceneDataCPU::EnumMaterialTextureSlot::OCCLUSION);
            result.opacity_texture = set_texture(
                DonutSceneDataCPU::EnumMaterialTextureSlot::OPACITY);
            return result;
        }

        DirectX::XMMATRIX load_float4x4(const DirectX::XMFLOAT4X4& matrix) {
            return DirectX::XMLoadFloat4x4(&matrix);
        }

        void include_bounds(SceneDataCPU& scene, const DirectX::XMFLOAT3& position) {
            scene.bounds_min.x = std::min(scene.bounds_min.x, position.x);
            scene.bounds_min.y = std::min(scene.bounds_min.y, position.y);
            scene.bounds_min.z = std::min(scene.bounds_min.z, position.z);
            scene.bounds_max.x = std::max(scene.bounds_max.x, position.x);
            scene.bounds_max.y = std::max(scene.bounds_max.y, position.y);
            scene.bounds_max.z = std::max(scene.bounds_max.z, position.z);
        }
    }

    std::unique_ptr<SceneDataCPU> SceneFastGltfImporter::load(
        const std::filesystem::path& path) {

        auto source = donut::DonutSceneFastGltfImporter::load(path);
        auto result = std::make_unique<SceneDataCPU>();
        result->source_path = source ? source->source_path : path;

        if (!source || !source->loaded) {
            result->error_message = source
                ? source->error_message
                : "fastgltf scene import failed.";
            return result;
        }

        result->bounds_min = { FLT_MAX, FLT_MAX, FLT_MAX };
        result->bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

        result->materials.reserve(source->materials.size());
        for (const DonutSceneDataCPU::Material& material : source->materials) {
            result->materials.push_back(convert_material(*source, material));
        }

        std::vector<uint32_t> submesh_to_mesh(
            source->submeshes.size(),
            (std::numeric_limits<uint32_t>::max)());
        result->meshes.reserve(source->submeshes.size());
        for (uint32_t submesh_id = 0;
            submesh_id < source->submeshes.size();
            ++submesh_id) {
            const DonutSceneDataCPU::Submesh& source_submesh =
                source->submeshes[submesh_id];

            SceneDataCPU::Mesh mesh{};
            mesh.name = source_submesh.name;
            mesh.vertex_start = static_cast<uint32_t>(result->vertices.size());
            mesh.index_start = static_cast<uint32_t>(result->indices.size());

            for (uint32_t vertex_index = 0;
                vertex_index < source_submesh.vertex_count;
                ++vertex_index) {
                const uint32_t source_vertex_index =
                    source_submesh.vertex_offset + vertex_index;

                SceneDataCPU::Vertex vertex{};
                vertex.position = source->positions[source_vertex_index];
                vertex.normal = unpack_snorm8_xyz(
                    source->packed_normals[source_vertex_index]);
                vertex.uv0 = source->texcoords[source_vertex_index];
                vertex.tangent = unpack_snorm8_xyz(
                    source->packed_tangents[source_vertex_index]);

                result->vertices.push_back(vertex);
                include_bounds(*result, vertex.position);
            }

            for (uint32_t index = 0;
                index < source_submesh.index_count;
                ++index) {
                result->indices.push_back(
                    source->indices[source_submesh.index_offset + index]);
            }

            mesh.vertex_count =
                static_cast<uint32_t>(result->vertices.size()) - mesh.vertex_start;
            mesh.index_count =
                static_cast<uint32_t>(result->indices.size()) - mesh.index_start;
            mesh.local_aabb = source_submesh.local_aabb;

            submesh_to_mesh[submesh_id] =
                static_cast<uint32_t>(result->meshes.size());
            result->meshes.emplace_back(std::move(mesh));
        }

        result->objects.reserve(source->geometry_instances.size());
        for (uint32_t geometry_instance_id = 0;
            geometry_instance_id < source->geometry_instances.size();
            ++geometry_instance_id) {
            const DonutSceneDataCPU::GeometryInstance& geometry_instance =
                source->geometry_instances[geometry_instance_id];
            if (geometry_instance.instance_id >= source->instances.size() ||
                geometry_instance.submesh_id >= submesh_to_mesh.size()) {
                continue;
            }

            const uint32_t mesh_index =
                submesh_to_mesh[geometry_instance.submesh_id];
            if (mesh_index >= result->meshes.size()) {
                continue;
            }

            const DonutSceneDataCPU::Submesh& submesh =
                source->submeshes[geometry_instance.submesh_id];
            const DonutSceneDataCPU::Instance& instance =
                source->instances[geometry_instance.instance_id];

            SceneDataCPU::Object object{};
            object.object_id = geometry_instance_id;
            object.material_index = submesh.material_id;
            object.mesh_index = mesh_index;
            object.flags = 0;
            DirectX::XMStoreFloat4x4(
                &object.transform,
                DirectX::XMMatrixTranspose(
                    load_float4x4(instance.world_transform)));
            result->objects.push_back(object);
        }

        if (result->vertices.empty() ||
            result->indices.empty() ||
            result->meshes.empty() ||
            result->objects.empty()) {
            result->error_message =
                "fastgltf scene had no renderable triangle meshes after conversion.";
            return result;
        }

        result->build_mesh_aabbs_from_vertices();
        result->build_batches_from_objects();
        result->loaded = true;
        result->error_message = "ok";
        return result;
    }
}
