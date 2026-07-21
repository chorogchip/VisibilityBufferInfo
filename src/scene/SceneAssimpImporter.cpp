#include "scene/SceneAssimpImporter.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <utility>
#include <iostream>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "util/Logger.h"

namespace scene {

    static DirectX::XMFLOAT4 fallback_color(uint32_t material_index) {
        static constexpr std::array<DirectX::XMFLOAT4, 8> colors = {
            DirectX::XMFLOAT4{0.80f, 0.22f, 0.18f, 1.0f},
            DirectX::XMFLOAT4{0.18f, 0.55f, 0.86f, 1.0f},
            DirectX::XMFLOAT4{0.26f, 0.72f, 0.34f, 1.0f},
            DirectX::XMFLOAT4{0.92f, 0.74f, 0.22f, 1.0f},
            DirectX::XMFLOAT4{0.66f, 0.36f, 0.84f, 1.0f},
            DirectX::XMFLOAT4{0.18f, 0.78f, 0.74f, 1.0f},
            DirectX::XMFLOAT4{0.86f, 0.42f, 0.22f, 1.0f},
            DirectX::XMFLOAT4{0.72f, 0.72f, 0.76f, 1.0f},
        };
        return colors[material_index % colors.size()];
    }

    static DirectX::XMFLOAT4 read_material_color(const aiMaterial* material, uint32_t material_index) {
        if (material == nullptr) return fallback_color(material_index);

        aiColor4D color = {};
        if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &color) ||
            AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color)) {
            return { color.r, color.g, color.b, color.a > 0.0f ? color.a : 1.0f };
        }
        return fallback_color(material_index);
    }

    static void read_first_texture_path(
        const aiMaterial* material,
        aiTextureType type,
        eng::MaterialCPU::TexturePath& destination) {
        if (material == nullptr || destination || material->GetTextureCount(type) == 0) return;

        aiString texture_path;
        if (material->GetTexture(type, 0, &texture_path) == AI_SUCCESS) {
            destination = std::filesystem::path(texture_path.C_Str());
        }
    }

    static void include_bounds(SceneDataCPU& scene, const DirectX::XMFLOAT3& position) {
        scene.bounds_min.x = std::min(scene.bounds_min.x, position.x);
        scene.bounds_min.y = std::min(scene.bounds_min.y, position.y);
        scene.bounds_min.z = std::min(scene.bounds_min.z, position.z);
        scene.bounds_max.x = std::max(scene.bounds_max.x, position.x);
        scene.bounds_max.y = std::max(scene.bounds_max.y, position.y);
        scene.bounds_max.z = std::max(scene.bounds_max.z, position.z);
    }

    std::unique_ptr<SceneDataCPU>  SceneAssimpImporter::load(const std::filesystem::path& path) {
        std::unique_ptr<SceneDataCPU> res{ new SceneDataCPU{} };
        res->source_path = path;

        Assimp::Importer importer;
        const unsigned int flags =
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenSmoothNormals |
            aiProcess_ImproveCacheLocality |
            aiProcess_PreTransformVertices |
            aiProcess_ConvertToLeftHanded |
            aiProcess_SortByPType;

        const aiScene* assimp_scene = importer.ReadFile(path.string(), flags);

        if (assimp_scene == nullptr || assimp_scene->mNumMeshes == 0) {

            util::Logger::g_logger <<
                "Error in Assimp Scene import: \n\t" <<
                importer.GetErrorString();

            util::Logger::g_logger.assert_with_log(false);
            return res;
        }
        util::Logger::g_logger <<
            "[Assimp Importer]: Read " << path.string() << "... \bparsing [" <<
            assimp_scene->mNumMaterials << "] materals | [" <<
            assimp_scene->mMeshes << "] meshes" << std::endl;

        res->bounds_min = { FLT_MAX, FLT_MAX, FLT_MAX };
        res->bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

        res->materials.reserve(assimp_scene->mNumMaterials);
        for (uint32_t material_index = 0; material_index < assimp_scene->mNumMaterials; ++material_index) {
            const aiMaterial* material = assimp_scene->mMaterials[material_index];
            eng::MaterialCPU imported_material;
            imported_material.base_color = read_material_color(material, material_index);

            if (material != nullptr) {
                aiString name;
                if (material->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
                    imported_material.name = name.C_Str();
                }
            }

            read_first_texture_path(material, aiTextureType_BASE_COLOR, imported_material.base_color_texture);
            read_first_texture_path(material, aiTextureType_DIFFUSE, imported_material.base_color_texture);
            read_first_texture_path(material, aiTextureType_NORMALS, imported_material.normal_texture);
            read_first_texture_path(material, aiTextureType_METALNESS, imported_material.metal_roughness_texture);
            read_first_texture_path(material, aiTextureType_DIFFUSE_ROUGHNESS, imported_material.metal_roughness_texture);
            read_first_texture_path(material, aiTextureType_EMISSIVE, imported_material.emissive_texture);
            read_first_texture_path(material, aiTextureType_AMBIENT_OCCLUSION, imported_material.occlusion_texture);
            read_first_texture_path(material, aiTextureType_LIGHTMAP, imported_material.occlusion_texture);
            read_first_texture_path(material, aiTextureType_OPACITY, imported_material.opacity_texture);

            res->materials.push_back(std::move(imported_material));
        }

        for (uint32_t mesh_index = 0; mesh_index < assimp_scene->mNumMeshes; ++mesh_index) {
            const aiMesh* mesh = assimp_scene->mMeshes[mesh_index];
            if (mesh == nullptr || mesh->mNumVertices == 0 || mesh->mNumFaces == 0) {
                continue;
            }

            SceneDataCPU::Mesh imported_mesh{};
            imported_mesh.name = mesh->mName.C_Str();
            imported_mesh.vertex_start = static_cast<uint32_t>(res->vertices.size());
            imported_mesh.index_start = static_cast<uint32_t>(res->indices.size());

            SceneDataCPU::Object imported_obj{};
            imported_obj.object_id = mesh_index;
            imported_obj.material_index = mesh->mMaterialIndex;
            imported_obj.mesh_index = mesh_index;
            imported_obj.flags = 0;

            const DirectX::XMFLOAT4 color =
                mesh->mMaterialIndex < res->materials.size()
                ? res->materials[mesh->mMaterialIndex].base_color
                : fallback_color(mesh->mMaterialIndex);

            for (uint32_t vertex_index = 0; vertex_index < mesh->mNumVertices; ++vertex_index) {
                const aiVector3D& position = mesh->mVertices[vertex_index];
                const aiVector3D normal =
                    mesh->HasNormals() ? mesh->mNormals[vertex_index] : aiVector3D(0.0f, 1.0f, 0.0f);
                const aiVector3D uv =
                    mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][vertex_index] : aiVector3D(0.0f, 0.0f, 0.0f);

                SceneDataCPU::Vertex vertex;
                vertex.position = { position.x, position.y, position.z };
                vertex.normal = { normal.x, normal.y, normal.z };
                vertex.uv0 = { uv.x, uv.y };
                vertex.tangent = DirectX::XMFLOAT3(color.x, color.y, color.z);
                // vertex.material_index = mesh->mMaterialIndex;
                res->vertices.push_back(vertex);
                include_bounds(*res, vertex.position);
            }

            for (uint32_t face_index = 0; face_index < mesh->mNumFaces; ++face_index) {
                const aiFace& face = mesh->mFaces[face_index];
                if (face.mNumIndices != 3) {
                    continue;
                }

                res->indices.push_back(face.mIndices[0]);
                res->indices.push_back(face.mIndices[1]);
                res->indices.push_back(face.mIndices[2]);
            }

            imported_mesh.vertex_count = static_cast<uint32_t>(res->vertices.size()) - imported_mesh.vertex_start;
            imported_mesh.index_count = static_cast<uint32_t>(res->indices.size()) - imported_mesh.index_start;
            DirectX::XMStoreFloat4x4(&imported_obj.transform, DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity()));
            if (imported_mesh.index_count > 0) {
                res->meshes.push_back(imported_mesh);
                res->objects.push_back(imported_obj);
            }
        }

        if (res->vertices.empty() || res->indices.empty() || res->meshes.empty()) {
            res->error_message = "Imported scene had no triangle meshes after filtering.";
            return res;
        }


        res->build_batches_from_objects();

        res->loaded = true;
        res->error_message = "ok";

        std::cout << "parsing complete" << std::endl;

        return res;
    }
}
