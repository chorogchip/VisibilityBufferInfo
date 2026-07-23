#include "scene/donut/DonutSceneAssimpImporter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/GltfMaterial.h>
#include <assimp/material.h>
#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace scene::donut {

    namespace {

        using MaterialTextureSlot = DonutSceneDataCPU::EnumMaterialTextureSlot;
        using TextureColorSpace = DonutSceneDataCPU::EnumTextureColorSpace;
        using TextureFallback = DonutSceneDataCPU::EnumTextureFallback;

        DirectX::XMFLOAT4 fallback_color(uint32_t material_index) {
            static constexpr std::array<DirectX::XMFLOAT4, 8> COLORS = {
                DirectX::XMFLOAT4{0.80f, 0.22f, 0.18f, 1.0f},
                DirectX::XMFLOAT4{0.18f, 0.55f, 0.86f, 1.0f},
                DirectX::XMFLOAT4{0.26f, 0.72f, 0.34f, 1.0f},
                DirectX::XMFLOAT4{0.92f, 0.74f, 0.22f, 1.0f},
                DirectX::XMFLOAT4{0.66f, 0.36f, 0.84f, 1.0f},
                DirectX::XMFLOAT4{0.18f, 0.78f, 0.74f, 1.0f},
                DirectX::XMFLOAT4{0.86f, 0.42f, 0.22f, 1.0f},
                DirectX::XMFLOAT4{0.72f, 0.72f, 0.76f, 1.0f}
            };
            return COLORS[material_index % COLORS.size()];
        }

        DirectX::XMFLOAT4X4 store_float4x4(DirectX::FXMMATRIX matrix) {
            DirectX::XMFLOAT4X4 result{};
            DirectX::XMStoreFloat4x4(&result, matrix);
            return result;
        }

        DirectX::XMMATRIX load_float4x4(const DirectX::XMFLOAT4X4& matrix) {
            return DirectX::XMLoadFloat4x4(&matrix);
        }

        DirectX::XMMATRIX convert_matrix(const aiMatrix4x4& value) {
            return DirectX::XMMATRIX(
                value.a1, value.b1, value.c1, value.d1,
                value.a2, value.b2, value.c2, value.d2,
                value.a3, value.b3, value.c3, value.d3,
                value.a4, value.b4, value.c4, value.d4);
        }

        DirectX::XMFLOAT3 normalize3(const DirectX::XMFLOAT3& value) {
            const float length = std::sqrt(
                value.x * value.x + value.y * value.y + value.z * value.z);
            if (length <= 0.0f) return { 0.0f, 1.0f, 0.0f };

            const float inv_length = 1.0f / length;
            return { value.x * inv_length, value.y * inv_length, value.z * inv_length };
        }

        uint32_t pack_snorm8(float x, float y, float z, float w) {
            const auto pack_component = [](float value) {
                const float clamped = std::clamp(value, -1.0f, 1.0f);
                const int32_t converted =
                    static_cast<int32_t>(std::round(clamped * 127.0f));
                return static_cast<uint32_t>(converted & 0xff);
            };

            return pack_component(x) |
                (pack_component(y) << 8) |
                (pack_component(z) << 16) |
                (pack_component(w) << 24);
        }

        DirectX::XMFLOAT4 read_material_color(
            const aiMaterial* material,
            uint32_t material_index) {
            if (material == nullptr) return fallback_color(material_index);

            aiColor4D color{};
            if (aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &color) == AI_SUCCESS ||
                aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS) {
                return {
                    color.r,
                    color.g,
                    color.b,
                    color.a > 0.0f ? color.a : 1.0f
                };
            }

            return fallback_color(material_index);
        }

        DirectX::XMFLOAT3 read_material_color3(
            const aiMaterial* material,
            const char* key,
            unsigned int type,
            unsigned int index,
            const DirectX::XMFLOAT3& fallback) {
            if (material == nullptr) return fallback;

            aiColor3D color{};
            if (material->Get(key, type, index, color) == AI_SUCCESS) {
                return { color.r, color.g, color.b };
            }

            return fallback;
        }

        float read_material_float(
            const aiMaterial* material,
            const char* key,
            unsigned int type,
            unsigned int index,
            float fallback) {
            float value = fallback;
            if (material != nullptr && material->Get(key, type, index, value) == AI_SUCCESS) {
                return value;
            }

            return fallback;
        }

        DonutSceneDataCPU::EnumMaterialDomain read_material_domain(
            const aiMaterial* material) {

            if (material == nullptr) {
                return DonutSceneDataCPU::EnumMaterialDomain::Opaque;
            }

            aiString alpha_mode;
            if (material->Get(AI_MATKEY_GLTF_ALPHAMODE, alpha_mode) != AI_SUCCESS) {
                return DonutSceneDataCPU::EnumMaterialDomain::Opaque;
            }

            const std::string mode = alpha_mode.C_Str();
            if (mode == "MASK") {
                return DonutSceneDataCPU::EnumMaterialDomain::AlphaTested;
            }
            if (mode == "BLEND") {
                return DonutSceneDataCPU::EnumMaterialDomain::AlphaBlended;
            }
            return DonutSceneDataCPU::EnumMaterialDomain::Opaque;
        }

        std::optional<std::filesystem::path> read_first_texture_path(
            const aiMaterial* material,
            aiTextureType type) {
            if (material == nullptr || material->GetTextureCount(type) == 0) {
                return std::nullopt;
            }

            aiString texture_path;
            if (material->GetTexture(type, 0, &texture_path) != AI_SUCCESS) {
                return std::nullopt;
            }

            return std::filesystem::path(texture_path.C_Str());
        }

        bool is_embedded_texture_path(const std::filesystem::path& path) {
            const auto native = path.native();
            return !native.empty() &&
                native[0] == static_cast<std::filesystem::path::value_type>('*');
        }

        TextureColorSpace color_space_for_slot(MaterialTextureSlot slot) {
            return slot == DonutSceneDataCPU::EnumMaterialTextureSlot::BASE_COLOR ||
                slot == DonutSceneDataCPU::EnumMaterialTextureSlot::EMISSIVE
                ? DonutSceneDataCPU::EnumTextureColorSpace::SRGB
                : DonutSceneDataCPU::EnumTextureColorSpace::LINEAR;
        }

        TextureFallback fallback_for_slot(MaterialTextureSlot slot) {
            switch (slot) {
            case DonutSceneDataCPU::EnumMaterialTextureSlot::BASE_COLOR:
            case DonutSceneDataCPU::EnumMaterialTextureSlot::METAL_ROUGHNESS:
            case DonutSceneDataCPU::EnumMaterialTextureSlot::OCCLUSION:
            case DonutSceneDataCPU::EnumMaterialTextureSlot::TRANSMISSION:
            case DonutSceneDataCPU::EnumMaterialTextureSlot::OPACITY:
                return DonutSceneDataCPU::EnumTextureFallback::WHITE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::NORMAL:
                return DonutSceneDataCPU::EnumTextureFallback::FLAT_NORMAL;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::EMISSIVE:
                return DonutSceneDataCPU::EnumTextureFallback::BLACK;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::COUNT:
                break;
            }
            return DonutSceneDataCPU::EnumTextureFallback::WHITE;
        }

        uint32_t register_texture(
            DonutSceneDataCPU& scene,
            std::unordered_map<std::string, uint32_t>& texture_cache,
            const std::filesystem::path& path,
            MaterialTextureSlot slot) {
            const TextureColorSpace color_space = color_space_for_slot(slot);
            const std::string cache_key = path.generic_string() +
                (color_space == DonutSceneDataCPU::EnumTextureColorSpace::SRGB ? "|srgb" : "|linear");
            const auto cached = texture_cache.find(cache_key);
            if (cached != texture_cache.end()) return cached->second;

            DonutSceneDataCPU::Texture texture{};
            texture.name = path.generic_string();
            texture.path = path;
            texture.color_space = color_space;
            texture.fallback = fallback_for_slot(slot);
            texture.embedded = is_embedded_texture_path(path);

            const uint32_t texture_id =
                static_cast<uint32_t>(scene.textures.size());
            scene.textures.emplace_back(std::move(texture));
            texture_cache.emplace(cache_key, texture_id);
            return texture_id;
        }

        void set_material_texture(
            DonutSceneDataCPU& scene,
            std::unordered_map<std::string, uint32_t>& texture_cache,
            DonutSceneDataCPU::Material& material,
            MaterialTextureSlot slot,
            const std::optional<std::filesystem::path>& path) {
            if (!path || path->empty()) return;

            material.texture_ids[static_cast<size_t>(slot)] =
                register_texture(scene, texture_cache, *path, slot);
        }

        DonutSceneDataCPU::Material make_default_material(uint32_t material_index) {
            DonutSceneDataCPU::Material material{};
            material.name = "material_" + std::to_string(material_index);
            material.base_color = fallback_color(material_index);
            material.base_color.w = 1.0f;
            return material;
        }

        DonutSceneDataCPU::Material import_material(
            DonutSceneDataCPU& scene,
            std::unordered_map<std::string, uint32_t>& texture_cache,
            const aiMaterial* source,
            uint32_t material_index) {

            DonutSceneDataCPU::Material material =
                make_default_material(material_index);

            const DirectX::XMFLOAT4 base_color =
                read_material_color(source, material_index);
            const float opacity = read_material_float(
                source, AI_MATKEY_OPACITY, base_color.w);
            material.base_color = {
                base_color.x,
                base_color.y,
                base_color.z,
                std::clamp(opacity, 0.0f, 1.0f)
            };
            material.emissive_color = read_material_color3(
                source, AI_MATKEY_COLOR_EMISSIVE, { 0.0f, 0.0f, 0.0f });
            material.metalness = read_material_float(
                source, AI_MATKEY_METALLIC_FACTOR, material.metalness);
            material.roughness = read_material_float(
                source, AI_MATKEY_ROUGHNESS_FACTOR, material.roughness);
            material.alpha_cutoff = std::clamp(
                read_material_float(
                    source, AI_MATKEY_GLTF_ALPHACUTOFF, material.alpha_cutoff),
                0.0f,
                1.0f);
            material.domain = read_material_domain(source);

            if (source != nullptr) {
                aiString name;
                if (source->Get(AI_MATKEY_NAME, name) == AI_SUCCESS && name.length > 0) {
                    material.name = name.C_Str();
                }

                int two_sided = 0;
                if (source->Get(AI_MATKEY_TWOSIDED, two_sided) == AI_SUCCESS) {
                    material.double_sided = two_sided != 0;
                }
            }

            std::optional<std::filesystem::path> base_color_path =
                read_first_texture_path(source, aiTextureType_BASE_COLOR);
            if (!base_color_path) {
                base_color_path =
                    read_first_texture_path(source, aiTextureType_DIFFUSE);
            }

            std::optional<std::filesystem::path> metal_roughness_path =
                read_first_texture_path(source, aiTextureType_METALNESS);
            if (!metal_roughness_path) {
                metal_roughness_path =
                    read_first_texture_path(source, aiTextureType_DIFFUSE_ROUGHNESS);
            }

            std::optional<std::filesystem::path> normal_path =
                read_first_texture_path(source, aiTextureType_NORMALS);
            if (!normal_path) {
                normal_path =
                    read_first_texture_path(source, aiTextureType_HEIGHT);
            }

            std::optional<std::filesystem::path> occlusion_path =
                read_first_texture_path(source, aiTextureType_AMBIENT_OCCLUSION);
            if (!occlusion_path) {
                occlusion_path =
                    read_first_texture_path(source, aiTextureType_LIGHTMAP);
            }

            set_material_texture(
                scene, texture_cache, material,
                DonutSceneDataCPU::EnumMaterialTextureSlot::BASE_COLOR, base_color_path);
            set_material_texture(
                scene, texture_cache, material,
                DonutSceneDataCPU::EnumMaterialTextureSlot::METAL_ROUGHNESS, metal_roughness_path);
            set_material_texture(
                scene, texture_cache, material,
                DonutSceneDataCPU::EnumMaterialTextureSlot::NORMAL, normal_path);
            set_material_texture(
                scene, texture_cache, material,
                DonutSceneDataCPU::EnumMaterialTextureSlot::EMISSIVE,
                read_first_texture_path(source, aiTextureType_EMISSIVE));
            set_material_texture(
                scene, texture_cache, material,
                DonutSceneDataCPU::EnumMaterialTextureSlot::OCCLUSION, occlusion_path);
            set_material_texture(
                scene, texture_cache, material,
                DonutSceneDataCPU::EnumMaterialTextureSlot::OPACITY,
                read_first_texture_path(source, aiTextureType_OPACITY));

            return material;
        }

        uint32_t append_node(
            DonutSceneDataCPU& scene,
            const std::string& name,
            uint32_t parent_id,
            DirectX::FXMMATRIX local_transform,
            DirectX::FXMMATRIX world_transform) {
            DonutSceneDataCPU::Node node{};
            node.name = name.empty()
                ? "node_" + std::to_string(scene.nodes.size())
                : name;
            node.parent_id = parent_id;
            node.local_transform = store_float4x4(local_transform);
            node.world_transform = store_float4x4(world_transform);

            const uint32_t node_id = static_cast<uint32_t>(scene.nodes.size());
            scene.nodes.emplace_back(std::move(node));
            if (parent_id != DonutSceneDataCPU::INVALID_INDEX) {
                scene.nodes[parent_id].children.push_back(node_id);
            } else {
                scene.root_node_id = node_id;
            }

            return node_id;
        }

        void add_instance(
            DonutSceneDataCPU& scene,
            uint32_t node_id,
            uint32_t mesh_id) {
            const DonutSceneDataCPU::Mesh& mesh = scene.meshes[mesh_id];
            DonutSceneDataCPU::Instance instance{};
            instance.node_id = node_id;
            instance.mesh_id = mesh_id;
            instance.first_geometry_instance =
                static_cast<uint32_t>(scene.geometry_instances.size());
            instance.geometry_instance_count = mesh.submesh_count;
            instance.world_transform = scene.nodes[node_id].world_transform;
            instance.prev_world_transform = instance.world_transform;
            instance.world_aabb =
                mesh.local_aabb.get_transformed(instance.world_transform);

            const uint32_t instance_id =
                static_cast<uint32_t>(scene.instances.size());
            scene.instances.emplace_back(std::move(instance));
            scene.nodes[node_id].instance_id = instance_id;

            for (uint32_t submesh_offset = 0;
                submesh_offset < mesh.submesh_count;
                ++submesh_offset) {
                scene.geometry_instances.push_back({
                    instance_id,
                    mesh.first_submesh + submesh_offset
                });
            }
        }

        math::AABB compute_subtree_aabb(
            DonutSceneDataCPU& scene,
            uint32_t node_id) {
            DonutSceneDataCPU::Node& node = scene.nodes[node_id];
            math::AABB result = math::AABB::create_empty();

            if (node.instance_id != DonutSceneDataCPU::INVALID_INDEX) {
                result = result.get_union(
                    scene.instances[node.instance_id].world_aabb);
            }

            const std::vector<uint32_t> children = node.children;
            for (uint32_t child_id : children) {
                result = result.get_union(compute_subtree_aabb(scene, child_id));
            }

            scene.nodes[node_id].subtree_world_aabb = result;
            return result;
        }

        void visit_node(
            DonutSceneDataCPU& scene,
            const aiNode* source_node,
            uint32_t parent_id,
            DirectX::FXMMATRIX parent_world_transform,
            const std::vector<uint32_t>& mesh_ids) {
            if (source_node == nullptr) return;

            const DirectX::XMMATRIX local_transform =
                convert_matrix(source_node->mTransformation);
            const DirectX::XMMATRIX world_transform =
                DirectX::XMMatrixMultiply(local_transform, parent_world_transform);

            const uint32_t node_id = append_node(
                scene,
                source_node->mName.C_Str(),
                parent_id,
                local_transform,
                world_transform);

            if (source_node->mNumMeshes == 1) {
                const uint32_t source_mesh_id = source_node->mMeshes[0];
                if (source_mesh_id < mesh_ids.size() &&
                    mesh_ids[source_mesh_id] != DonutSceneDataCPU::INVALID_INDEX) {
                    add_instance(scene, node_id, mesh_ids[source_mesh_id]);
                }
            } else {
                for (uint32_t node_mesh_index = 0;
                    node_mesh_index < source_node->mNumMeshes;
                    ++node_mesh_index) {
                    const uint32_t source_mesh_id =
                        source_node->mMeshes[node_mesh_index];
                    if (source_mesh_id >= mesh_ids.size() ||
                        mesh_ids[source_mesh_id] == DonutSceneDataCPU::INVALID_INDEX) {
                        continue;
                    }

                    const uint32_t attachment_node_id = append_node(
                        scene,
                        std::string(source_node->mName.C_Str()) +
                        "_mesh_" + std::to_string(node_mesh_index),
                        node_id,
                        DirectX::XMMatrixIdentity(),
                        world_transform);
                    add_instance(scene, attachment_node_id, mesh_ids[source_mesh_id]);
                }
            }

            for (uint32_t child_index = 0;
                child_index < source_node->mNumChildren;
                ++child_index) {
                visit_node(
                    scene,
                    source_node->mChildren[child_index],
                    node_id,
                    world_transform,
                    mesh_ids);
            }
        }
    }

    std::unique_ptr<DonutSceneDataCPU> DonutSceneAssimpImporter::load(
        const std::filesystem::path& path) {
        auto result = std::make_unique<DonutSceneDataCPU>();
        result->source_path = std::filesystem::absolute(path).lexically_normal();

        Assimp::Importer importer;
        const unsigned int flags =
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_ImproveCacheLocality |
            aiProcess_ConvertToLeftHanded |
            aiProcess_SortByPType;

        const aiScene* imported_scene = importer.ReadFile(
            result->source_path.string(), flags);
        if (imported_scene == nullptr || imported_scene->mNumMeshes == 0) {
            result->error_message = importer.GetErrorString();
            if (result->error_message.empty()) {
                result->error_message = "Assimp returned no meshes.";
            }
            return result;
        }

        std::unordered_map<std::string, uint32_t> texture_cache;
        result->materials.reserve(std::max(1u, imported_scene->mNumMaterials));
        for (uint32_t material_index = 0; material_index < imported_scene->mNumMaterials; ++material_index) {
            result->materials.push_back(import_material(
                *result,
                texture_cache,
                imported_scene->mMaterials[material_index],
                material_index));
        }
        if (result->materials.empty()) {
            result->materials.push_back(make_default_material(0));
        }

        std::vector<uint32_t> mesh_ids(
            imported_scene->mNumMeshes,
            DonutSceneDataCPU::INVALID_INDEX);
        for (uint32_t source_mesh_id = 0;
            source_mesh_id < imported_scene->mNumMeshes;
            ++source_mesh_id) {
            const aiMesh* source_mesh = imported_scene->mMeshes[source_mesh_id];
            if (source_mesh == nullptr ||
                source_mesh->mNumVertices == 0 ||
                source_mesh->mNumFaces == 0) {
                continue;
            }

            const uint32_t vertex_offset =
                static_cast<uint32_t>(result->positions.size());
            const uint32_t index_offset =
                static_cast<uint32_t>(result->indices.size());
            math::AABB submesh_aabb = math::AABB::create_empty();

            for (uint32_t vertex_index = 0;
                vertex_index < source_mesh->mNumVertices;
                ++vertex_index) {
                const aiVector3D& source_position =
                    source_mesh->mVertices[vertex_index];
                const aiVector3D source_normal = source_mesh->HasNormals()
                    ? source_mesh->mNormals[vertex_index]
                    : aiVector3D(0.0f, 1.0f, 0.0f);
                const aiVector3D source_texcoord = source_mesh->HasTextureCoords(0)
                    ? source_mesh->mTextureCoords[0][vertex_index]
                    : aiVector3D(0.0f, 0.0f, 0.0f);
                const aiVector3D source_tangent =
                    source_mesh->HasTangentsAndBitangents()
                    ? source_mesh->mTangents[vertex_index]
                    : aiVector3D(1.0f, 0.0f, 0.0f);

                float tangent_sign = 1.0f;
                if (source_mesh->HasTangentsAndBitangents()) {
                    const aiVector3D cross = source_normal ^ source_tangent;
                    tangent_sign =
                        (cross * source_mesh->mBitangents[vertex_index]) > 0.0f
                        ? -1.0f
                        : 1.0f;
                }

                const DirectX::XMFLOAT3 position = {
                    source_position.x,
                    source_position.y,
                    source_position.z
                };
                const DirectX::XMFLOAT3 normal =
                    normalize3({ source_normal.x, source_normal.y, source_normal.z });
                const DirectX::XMFLOAT3 tangent =
                    normalize3({ source_tangent.x, source_tangent.y, source_tangent.z });

                result->positions.push_back(position);
                result->prev_positions.push_back(position);
                result->texcoords.push_back({ source_texcoord.x, source_texcoord.y });
                result->packed_normals.push_back(pack_snorm8(
                    normal.x, normal.y, normal.z, 0.0f));
                result->packed_tangents.push_back(pack_snorm8(
                    tangent.x, tangent.y, tangent.z, tangent_sign));

                submesh_aabb =
                    submesh_aabb.get_union(math::AABB::create_from_pos(position));
            }

            for (uint32_t face_index = 0;
                face_index < source_mesh->mNumFaces;
                ++face_index) {
                const aiFace& face = source_mesh->mFaces[face_index];
                if (face.mNumIndices != 3) continue;

                result->indices.push_back(face.mIndices[0]);
                result->indices.push_back(face.mIndices[1]);
                result->indices.push_back(face.mIndices[2]);
            }

            const uint32_t vertex_count =
                static_cast<uint32_t>(result->positions.size()) - vertex_offset;
            const uint32_t index_count =
                static_cast<uint32_t>(result->indices.size()) - index_offset;
            if (index_count == 0) {
                result->positions.resize(vertex_offset);
                result->prev_positions.resize(vertex_offset);
                result->texcoords.resize(vertex_offset);
                result->packed_normals.resize(vertex_offset);
                result->packed_tangents.resize(vertex_offset);
                result->indices.resize(index_offset);
                continue;
            }

            DonutSceneDataCPU::Submesh submesh{};
            submesh.name = source_mesh->mName.C_Str();
            submesh.vertex_offset = vertex_offset;
            submesh.vertex_count = vertex_count;
            submesh.index_offset = index_offset;
            submesh.index_count = index_count;
            submesh.material_id =
                source_mesh->mMaterialIndex < result->materials.size()
                ? source_mesh->mMaterialIndex
                : 0;
            submesh.local_aabb = submesh_aabb;

            DonutSceneDataCPU::Mesh mesh{};
            mesh.name = submesh.name.empty()
                ? "mesh_" + std::to_string(source_mesh_id)
                : submesh.name;
            mesh.first_submesh =
                static_cast<uint32_t>(result->submeshes.size());
            mesh.submesh_count = 1;
            mesh.local_aabb = submesh.local_aabb;

            const uint32_t mesh_id =
                static_cast<uint32_t>(result->meshes.size());
            result->submeshes.emplace_back(std::move(submesh));
            result->meshes.emplace_back(std::move(mesh));
            mesh_ids[source_mesh_id] = mesh_id;
        }

        if (result->meshes.empty()) {
            result->error_message =
                "Imported scene had no triangle meshes after filtering.";
            return result;
        }

        if (imported_scene->mRootNode != nullptr) {
            visit_node(
                *result,
                imported_scene->mRootNode,
                DonutSceneDataCPU::INVALID_INDEX,
                DirectX::XMMatrixIdentity(),
                mesh_ids);
        }

        if (result->nodes.empty()) {
            append_node(
                *result,
                "root",
                DonutSceneDataCPU::INVALID_INDEX,
                DirectX::XMMatrixIdentity(),
                DirectX::XMMatrixIdentity());
        }

        if (result->instances.empty()) {
            for (uint32_t mesh_id = 0; mesh_id < result->meshes.size(); ++mesh_id) {
                const uint32_t node_id = append_node(
                    *result,
                    "mesh_" + std::to_string(mesh_id),
                    result->root_node_id,
                    DirectX::XMMatrixIdentity(),
                    load_float4x4(result->nodes[result->root_node_id].world_transform));
                add_instance(*result, node_id, mesh_id);
            }
        }

        result->world_aabb =
            compute_subtree_aabb(*result, result->root_node_id);

        std::string validation_error;
        if (!result->validate(validation_error)) {
            result->error_message = std::move(validation_error);
            return result;
        }

        result->loaded = true;
        result->error_message = "ok";
        return result;
    }
}
