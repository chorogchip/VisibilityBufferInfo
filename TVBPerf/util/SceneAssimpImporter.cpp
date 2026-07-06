#include "SceneAssimpImporter.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <utility>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

static DirectX::XMFLOAT4 fallback_color(uint32_t material_index)
{
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
    if (material == nullptr) {
        return fallback_color(material_index);
    }

    aiColor4D color = {};
    if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &color) ||
        AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color)) {
        return {color.r, color.g, color.b, color.a > 0.0f ? color.a : 1.0f};
    }

    return fallback_color(material_index);
}

static void append_texture_paths(
    const aiMaterial* material,
    aiTextureType type,
    ImportedScene::ImportedSceneMaterial & imported_material)
{
    if (material == nullptr) {
        return;
    }

    const uint32_t count = material->GetTextureCount(type);
    for (uint32_t texture_index = 0; texture_index < count; ++texture_index) {
        aiString texture_path;
        if (material->GetTexture(type, texture_index, &texture_path) == AI_SUCCESS) {
            imported_material.texture_paths.emplace_back(texture_path.C_Str());
        }
    }
}

static void include_bounds(ImportedScene& scene, const DirectX::XMFLOAT3& position) {
    scene.bounds_min.x = std::min(scene.bounds_min.x, position.x);
    scene.bounds_min.y = std::min(scene.bounds_min.y, position.y);
    scene.bounds_min.z = std::min(scene.bounds_min.z, position.z);
    scene.bounds_max.x = std::max(scene.bounds_max.x, position.x);
    scene.bounds_max.y = std::max(scene.bounds_max.y, position.y);
    scene.bounds_max.z = std::max(scene.bounds_max.z, position.z);
}

std::unique_ptr<ImportedSceneLoadResult> load_imported_scene_with_assimp(const std::filesystem::path& path) {
    std::unique_ptr<ImportedSceneLoadResult>result{ new ImportedSceneLoadResult{} };

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
        result->message = importer.GetErrorString();
        if (result->message.empty()) {
            result->message = "Assimp returned no meshes.";
        }
        return result;
    }

    ImportedScene scene;
    scene.source_path = path;
    scene.bounds_min = {FLT_MAX, FLT_MAX, FLT_MAX};
    scene.bounds_max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    scene.materials.reserve(assimp_scene->mNumMaterials);
    for (uint32_t material_index = 0; material_index < assimp_scene->mNumMaterials; ++material_index) {
        const aiMaterial* material = assimp_scene->mMaterials[material_index];
        ImportedScene::ImportedSceneMaterial imported_material;
        imported_material.base_color = read_material_color(material, material_index);

        if (material != nullptr) {
            aiString name;
            if (material->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
                imported_material.name = name.C_Str();
            }
        }

        append_texture_paths(material, aiTextureType_DIFFUSE, imported_material);
        append_texture_paths(material, aiTextureType_BASE_COLOR, imported_material);
        append_texture_paths(material, aiTextureType_NORMALS, imported_material);
        append_texture_paths(material, aiTextureType_SPECULAR, imported_material);
        append_texture_paths(material, aiTextureType_METALNESS, imported_material);
        append_texture_paths(material, aiTextureType_DIFFUSE_ROUGHNESS, imported_material);
        append_texture_paths(material, aiTextureType_EMISSIVE, imported_material);
        append_texture_paths(material, aiTextureType_OPACITY, imported_material);

        scene.materials.push_back(std::move(imported_material));
    }

    for (uint32_t mesh_index = 0; mesh_index < assimp_scene->mNumMeshes; ++mesh_index) {
        const aiMesh* mesh = assimp_scene->mMeshes[mesh_index];
        if (mesh == nullptr || mesh->mNumVertices == 0 || mesh->mNumFaces == 0) {
            continue;
        }

        ImportedScene::ImportedSceneMesh imported_mesh;
        imported_mesh.name = mesh->mName.C_Str();
        imported_mesh.vertex_start = static_cast<uint32_t>(scene.vertices.size());
        imported_mesh.index_start = static_cast<uint32_t>(scene.indices.size());
        imported_mesh.material_index = mesh->mMaterialIndex;

        const DirectX::XMFLOAT4 color =
            mesh->mMaterialIndex < scene.materials.size()
                ? scene.materials[mesh->mMaterialIndex].base_color
                : fallback_color(mesh->mMaterialIndex);

        for (uint32_t vertex_index = 0; vertex_index < mesh->mNumVertices; ++vertex_index) {
            const aiVector3D& position = mesh->mVertices[vertex_index];
            const aiVector3D normal =
                mesh->HasNormals() ? mesh->mNormals[vertex_index] : aiVector3D(0.0f, 1.0f, 0.0f);
            const aiVector3D uv =
                mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][vertex_index] : aiVector3D(0.0f, 0.0f, 0.0f);

            ImportedScene::ImportedSceneVertex vertex;
            vertex.position = {position.x, position.y, position.z};
            vertex.normal = {normal.x, normal.y, normal.z};
            vertex.uv = {uv.x, uv.y};
            vertex.color = color;
            vertex.material_index = mesh->mMaterialIndex;
            scene.vertices.push_back(vertex);
            include_bounds(scene, vertex.position);
        }

        for (uint32_t face_index = 0; face_index < mesh->mNumFaces; ++face_index) {
            const aiFace& face = mesh->mFaces[face_index];
            if (face.mNumIndices != 3) {
                continue;
            }

            scene.indices.push_back(imported_mesh.vertex_start + face.mIndices[0]);
            scene.indices.push_back(imported_mesh.vertex_start + face.mIndices[1]);
            scene.indices.push_back(imported_mesh.vertex_start + face.mIndices[2]);
        }

        imported_mesh.vertex_count = static_cast<uint32_t>(scene.vertices.size()) - imported_mesh.vertex_start;
        imported_mesh.index_count = static_cast<uint32_t>(scene.indices.size()) - imported_mesh.index_start;
        if (imported_mesh.index_count > 0) {
            scene.meshes.push_back(imported_mesh);
        }
    }

    if (scene.vertices.empty() || scene.indices.empty() || scene.meshes.empty()) {
        result->message = "Imported scene had no triangle meshes after filtering.";
        return result;
    }

    result->loaded = true;
    result->message = "ok";
    result->scene = std::move(scene);
    return result;
}

std::unique_ptr<SceneSynthSphereRuntime> scene_loaded_scene_to_cpu_scene(const ImportedSceneLoadResult& src) {
    std::unique_ptr<SceneSynthSphereRuntime> ret{ new SceneSynthSphereRuntime {} };

    // TOOD 이걸 3시간만에 만들수있을까?
    
    for (const auto& vert : src.scene.vertices) {
        ret->vertex_buffer;
    }

    for (const auto& mesh : src.scene.meshes) {
        mesh;
    }

    return ret;
}