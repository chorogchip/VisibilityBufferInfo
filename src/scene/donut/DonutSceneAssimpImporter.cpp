#include "scene/donut/DonutSceneAssimpImporter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace scene::donut {

    namespace {

        DirectX::XMFLOAT4 fallback_color(uint32_t material_index) {
            static constexpr std::array<DirectX::XMFLOAT4, 8> colors = {
                DirectX::XMFLOAT4{0.80f, 0.22f, 0.18f, 1.0f},
                DirectX::XMFLOAT4{0.18f, 0.55f, 0.86f, 1.0f},
                DirectX::XMFLOAT4{0.26f, 0.72f, 0.34f, 1.0f},
                DirectX::XMFLOAT4{0.92f, 0.74f, 0.22f, 1.0f},
                DirectX::XMFLOAT4{0.66f, 0.36f, 0.84f, 1.0f},
                DirectX::XMFLOAT4{0.18f, 0.78f, 0.74f, 1.0f},
                DirectX::XMFLOAT4{0.86f, 0.42f, 0.22f, 1.0f},
                DirectX::XMFLOAT4{0.72f, 0.72f, 0.76f, 1.0f}
            };
            return colors[material_index % colors.size()];
        }

        uint32_t pack_snorm8(const DirectX::XMFLOAT4& value) {
            const float length = std::sqrt(
                value.x * value.x + value.y * value.y + value.z * value.z);
            const float scale = length > 0.0f ? 127.0f / length : 0.0f;
            const int32_t x = static_cast<int32_t>(value.x * scale);
            const int32_t y = static_cast<int32_t>(value.y * scale);
            const int32_t z = static_cast<int32_t>(value.z * scale);
            const int32_t w = static_cast<int32_t>(value.w * scale);
            return static_cast<uint32_t>(x & 0xff) |
                (static_cast<uint32_t>(y & 0xff) << 8) |
                (static_cast<uint32_t>(z & 0xff) << 16) |
                (static_cast<uint32_t>(w & 0xff) << 24);
        }

        DirectX::XMFLOAT4 read_material_color(const aiMaterial* material, uint32_t material_index) {
            if (material == nullptr) return fallback_color(material_index);

            aiColor4D color{};
            if (aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &color) == AI_SUCCESS ||
                aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS) {
                return {color.r, color.g, color.b, color.a > 0.0f ? color.a : 1.0f};
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
                return {color.r, color.g, color.b};
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

        void read_first_texture_path(
            const aiMaterial* material,
            aiTextureType type,
            DonutSceneDataCPU::TexturePath& destination) {
            if (material == nullptr || destination || material->GetTextureCount(type) == 0) return;

            aiString texture_path;
            if (material->GetTexture(type, 0, &texture_path) == AI_SUCCESS) {
                destination = std::filesystem::path(texture_path.C_Str());
            }
        }

        void set_texture_flag(
            DonutSceneDataCPU::Material& material,
            MaterialTextureSlot slot,
            int32_t flag) {
            if (material.textures[static_cast<size_t>(slot)]) {
                material.constants.flags |= flag;
            }
        }

        DonutSceneDataCPU::Material make_default_material(uint32_t material_index) {
            DonutSceneDataCPU::Material material{};
            material.name = "material_" + std::to_string(material_index);
            material.constants.baseOrDiffuseColor = {1.0f, 1.0f, 1.0f};
            material.constants.specularColor = {1.0f, 1.0f, 1.0f};
            material.constants.emissiveColor = {0.0f, 0.0f, 0.0f};
            material.constants.materialID = static_cast<int32_t>(material_index);
            material.constants.domain = material_domain_opaque;
            material.constants.opacity = 1.0f;
            material.constants.roughness = 1.0f;
            material.constants.metalness = 0.0f;
            material.constants.normalTextureScale = 1.0f;
            material.constants.occlusionStrength = 1.0f;
            material.constants.alphaCutoff = 0.5f;
            material.constants.transmissionFactor = 0.0f;
            material.constants.baseOrDiffuseTextureIndex = invalid_descriptor_index;
            material.constants.metalRoughOrSpecularTextureIndex = invalid_descriptor_index;
            material.constants.emissiveTextureIndex = invalid_descriptor_index;
            material.constants.normalTextureIndex = invalid_descriptor_index;
            material.constants.occlusionTextureIndex = invalid_descriptor_index;
            material.constants.transmissionTextureIndex = invalid_descriptor_index;
            material.constants.opacityTextureIndex = invalid_descriptor_index;
            material.constants.normalTextureTransformScale = {1.0f, 1.0f};
            material.constants.sssTransmissionColor = {1.0f, 1.0f, 1.0f};
            material.constants.sssScatteringColor = {0.0f, 0.0f, 0.0f};
            material.constants.hairBaseColor = {0.0f, 0.0f, 0.0f};
            material.constants.hairIor = 1.55f;
            material.constants.hairDiffuseReflectionTint = {1.0f, 1.0f, 1.0f};
            return material;
        }

        DonutSceneDataCPU::Material import_material(const aiMaterial* source, uint32_t material_index) {
            DonutSceneDataCPU::Material material = make_default_material(material_index);
            const DirectX::XMFLOAT4 base_color = read_material_color(source, material_index);
            material.constants.baseOrDiffuseColor = {base_color.x, base_color.y, base_color.z};
            material.constants.opacity = base_color.w;
            material.constants.specularColor = read_material_color3(
                source, AI_MATKEY_COLOR_SPECULAR, {1.0f, 1.0f, 1.0f});
            material.constants.emissiveColor = read_material_color3(
                source, AI_MATKEY_COLOR_EMISSIVE, {0.0f, 0.0f, 0.0f});
            material.constants.metalness = read_material_float(
                source, AI_MATKEY_METALLIC_FACTOR, material.constants.metalness);
            material.constants.roughness = read_material_float(
                source, AI_MATKEY_ROUGHNESS_FACTOR, material.constants.roughness);
            material.constants.opacity = read_material_float(
                source, AI_MATKEY_OPACITY, material.constants.opacity);
            material.constants.alphaCutoff = read_material_float(
                source, "$mat.gltf.alphaCutoff", 0, 0, material.constants.alphaCutoff);
            material.constants.transmissionFactor = read_material_float(
                source, AI_MATKEY_TRANSMISSION_FACTOR, material.constants.transmissionFactor);

            if (source != nullptr) {
                aiString name;
                if (source->Get(AI_MATKEY_NAME, name) == AI_SUCCESS && name.length > 0) {
                    material.name = name.C_Str();
                }

                int two_sided = 0;
                if (source->Get(AI_MATKEY_TWOSIDED, two_sided) == AI_SUCCESS && two_sided != 0) {
                    material.constants.flags |= material_flag_double_sided;
                }
            }

            auto& base = material.textures[static_cast<size_t>(MaterialTextureSlot::base_or_diffuse)];
            auto& metal_rough = material.textures[static_cast<size_t>(MaterialTextureSlot::metal_rough_or_specular)];
            auto& normal = material.textures[static_cast<size_t>(MaterialTextureSlot::normal)];
            auto& emissive = material.textures[static_cast<size_t>(MaterialTextureSlot::emissive)];
            auto& occlusion = material.textures[static_cast<size_t>(MaterialTextureSlot::occlusion)];
            auto& transmission = material.textures[static_cast<size_t>(MaterialTextureSlot::transmission)];
            auto& opacity = material.textures[static_cast<size_t>(MaterialTextureSlot::opacity)];

            read_first_texture_path(source, aiTextureType_BASE_COLOR, base);
            read_first_texture_path(source, aiTextureType_DIFFUSE, base);
            read_first_texture_path(source, aiTextureType_METALNESS, metal_rough);
            read_first_texture_path(source, aiTextureType_DIFFUSE_ROUGHNESS, metal_rough);
            read_first_texture_path(source, aiTextureType_SPECULAR, metal_rough);
            read_first_texture_path(source, aiTextureType_NORMALS, normal);
            read_first_texture_path(source, aiTextureType_HEIGHT, normal);
            read_first_texture_path(source, aiTextureType_EMISSIVE, emissive);
            read_first_texture_path(source, aiTextureType_AMBIENT_OCCLUSION, occlusion);
            read_first_texture_path(source, aiTextureType_LIGHTMAP, occlusion);
            read_first_texture_path(source, aiTextureType_TRANSMISSION, transmission);
            read_first_texture_path(source, aiTextureType_OPACITY, opacity);

            set_texture_flag(material, MaterialTextureSlot::base_or_diffuse, material_flag_use_base_or_diffuse);
            set_texture_flag(material, MaterialTextureSlot::metal_rough_or_specular, material_flag_use_metal_rough_or_specular);
            set_texture_flag(material, MaterialTextureSlot::normal, material_flag_use_normal);
            set_texture_flag(material, MaterialTextureSlot::emissive, material_flag_use_emissive);
            set_texture_flag(material, MaterialTextureSlot::occlusion, material_flag_use_occlusion);
            set_texture_flag(material, MaterialTextureSlot::transmission, material_flag_use_transmission);
            set_texture_flag(material, MaterialTextureSlot::opacity, material_flag_use_opacity);

            if (material.constants.transmissionFactor > 0.0f || transmission) {
                material.constants.domain = material_domain_transmissive;
            } else if (material.constants.opacity < 1.0f || opacity) {
                material.constants.domain = material_domain_alpha_blended;
            }

            return material;
        }

        DirectX::XMMATRIX convert_matrix(const aiMatrix4x4& value) {
            return DirectX::XMMATRIX(
                value.a1, value.a2, value.a3, value.a4,
                value.b1, value.b2, value.b3, value.b4,
                value.c1, value.c2, value.c3, value.c4,
                value.d1, value.d2, value.d3, value.d4);
        }

        DirectX::XMFLOAT3X4 to_float3x4(const DirectX::XMMATRIX& value) {
            DirectX::XMFLOAT4X4 matrix{};
            DirectX::XMStoreFloat4x4(&matrix, value);
            return {
                matrix._11, matrix._12, matrix._13, matrix._14,
                matrix._21, matrix._22, matrix._23, matrix._24,
                matrix._31, matrix._32, matrix._33, matrix._34
            };
        }

        void add_instance(
            DonutSceneDataCPU& scene,
            uint32_t mesh_index,
            const DirectX::XMMATRIX& transform) {
            const DonutSceneDataCPU::Mesh& mesh = scene.meshes[mesh_index];
            DonutSceneDataCPU::Instance instance{};
            instance.mesh_index = mesh_index;
            instance.first_geometry_instance = static_cast<uint32_t>(scene.draws.size());
            instance.transform = to_float3x4(transform);
            instance.prev_transform = instance.transform;
            const uint32_t instance_index = static_cast<uint32_t>(scene.instances.size());
            scene.instances.push_back(instance);

            for (uint32_t geometry_offset = 0; geometry_offset < mesh.geometry_count; ++geometry_offset) {
                scene.draws.push_back({mesh.first_geometry + geometry_offset, instance_index});
            }
        }

        void visit_node(
            DonutSceneDataCPU& scene,
            const aiNode* node,
            const std::vector<uint32_t>& mesh_indices,
            const DirectX::XMMATRIX& parent_transform) {
            if (node == nullptr) return;

            const DirectX::XMMATRIX transform = DirectX::XMMatrixMultiply(
                parent_transform, convert_matrix(node->mTransformation));

            for (uint32_t node_mesh_index = 0; node_mesh_index < node->mNumMeshes; ++node_mesh_index) {
                const uint32_t source_mesh_index = node->mMeshes[node_mesh_index];
                if (source_mesh_index < mesh_indices.size() &&
                    mesh_indices[source_mesh_index] != invalid_index) {
                    add_instance(scene, mesh_indices[source_mesh_index], transform);
                }
            }

            for (uint32_t child_index = 0; child_index < node->mNumChildren; ++child_index) {
                visit_node(scene, node->mChildren[child_index], mesh_indices, transform);
            }
        }
    }

    std::unique_ptr<DonutSceneDataCPU> DonutSceneAssimpImporter::load(
        const std::filesystem::path& path) {
        auto result = std::make_unique<DonutSceneDataCPU>();
        result->source_path = path;

        Assimp::Importer importer;
        const unsigned int flags =
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_ImproveCacheLocality |
            aiProcess_ConvertToLeftHanded |
            aiProcess_SortByPType;

        const aiScene* imported_scene = importer.ReadFile(path.string(), flags);
        if (imported_scene == nullptr || imported_scene->mNumMeshes == 0) {
            result->error_message = importer.GetErrorString();
            if (result->error_message.empty()) {
                result->error_message = "Assimp returned no meshes.";
            }
            return result;
        }

        result->materials.reserve(std::max(1u, imported_scene->mNumMaterials));
        for (uint32_t material_index = 0; material_index < imported_scene->mNumMaterials; ++material_index) {
            result->materials.push_back(import_material(
                imported_scene->mMaterials[material_index], material_index));
        }
        if (result->materials.empty()) {
            result->materials.push_back(make_default_material(0));
        }

        std::vector<uint32_t> mesh_indices(imported_scene->mNumMeshes, invalid_index);
        for (uint32_t source_mesh_index = 0;
            source_mesh_index < imported_scene->mNumMeshes;
            ++source_mesh_index) {
            const aiMesh* source_mesh = imported_scene->mMeshes[source_mesh_index];
            if (source_mesh == nullptr || source_mesh->mNumVertices == 0 || source_mesh->mNumFaces == 0) {
                continue;
            }

            DonutSceneDataCPU::Geometry geometry{};
            geometry.name = source_mesh->mName.C_Str();
            geometry.vertex_offset = static_cast<uint32_t>(result->positions.size());
            geometry.index_offset = static_cast<uint32_t>(result->indices.size());
            geometry.material_index = source_mesh->mMaterialIndex < result->materials.size()
                ? source_mesh->mMaterialIndex : 0;

            for (uint32_t vertex_index = 0; vertex_index < source_mesh->mNumVertices; ++vertex_index) {
                const aiVector3D& position = source_mesh->mVertices[vertex_index];
                const aiVector3D normal = source_mesh->HasNormals()
                    ? source_mesh->mNormals[vertex_index]
                    : aiVector3D(0.0f, 1.0f, 0.0f);
                const aiVector3D texcoord = source_mesh->HasTextureCoords(0)
                    ? source_mesh->mTextureCoords[0][vertex_index]
                    : aiVector3D(0.0f, 0.0f, 0.0f);
                const aiVector3D tangent = source_mesh->HasTangentsAndBitangents()
                    ? source_mesh->mTangents[vertex_index]
                    : aiVector3D(1.0f, 0.0f, 0.0f);

                float tangent_sign = 1.0f;
                if (source_mesh->HasTangentsAndBitangents()) {
                    const aiVector3D cross = normal ^ tangent;
                    tangent_sign = (cross * source_mesh->mBitangents[vertex_index]) > 0.0f
                        ? -1.0f : 1.0f;
                }

                result->positions.push_back({position.x, position.y, position.z});
                result->prev_positions.push_back({position.x, position.y, position.z});
                result->texcoords.push_back({texcoord.x, texcoord.y});
                result->packed_normals.push_back(pack_snorm8({normal.x, normal.y, normal.z, 0.0f}));
                result->packed_tangents.push_back(pack_snorm8({
                    tangent.x, tangent.y, tangent.z, tangent_sign}));
            }

            for (uint32_t face_index = 0; face_index < source_mesh->mNumFaces; ++face_index) {
                const aiFace& face = source_mesh->mFaces[face_index];
                if (face.mNumIndices != 3) continue;

                result->indices.push_back(face.mIndices[0]);
                result->indices.push_back(face.mIndices[1]);
                result->indices.push_back(face.mIndices[2]);
            }

            geometry.vertex_count = static_cast<uint32_t>(result->positions.size()) - geometry.vertex_offset;
            geometry.index_count = static_cast<uint32_t>(result->indices.size()) - geometry.index_offset;
            if (geometry.index_count == 0) continue;

            DonutSceneDataCPU::Mesh mesh{};
            mesh.name = geometry.name.empty()
                ? "mesh_" + std::to_string(source_mesh_index)
                : geometry.name;
            mesh.first_geometry = static_cast<uint32_t>(result->geometries.size());
            mesh.geometry_count = 1;
            result->geometries.push_back(std::move(geometry));
            mesh_indices[source_mesh_index] = static_cast<uint32_t>(result->meshes.size());
            result->meshes.push_back(std::move(mesh));
        }

        if (result->meshes.empty()) {
            result->error_message = "Imported scene had no triangle meshes after filtering.";
            return result;
        }

        visit_node(*result, imported_scene->mRootNode, mesh_indices, DirectX::XMMatrixIdentity());
        if (result->instances.empty()) {
            for (uint32_t mesh_index = 0; mesh_index < result->meshes.size(); ++mesh_index) {
                add_instance(*result, mesh_index, DirectX::XMMatrixIdentity());
            }
        }

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
