#include "scene/builder/source/AssimpSceneSourceBuilder.h"

#include <algorithm>
#include <cctype>
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

#include <DirectXMath.h>

#include "util/Logger.h"
#include "scene/builder/source/SceneSourceDataValidator.h"

namespace scene {

    namespace {

        struct TextureInput {
            std::filesystem::path path;
            uint32_t uv_set = 0;
        };

        DirectX::XMFLOAT4X4 convert_matrix(const aiMatrix4x4& value) {
            return {
                value.a1, value.b1, value.c1, value.d1,
                value.a2, value.b2, value.c2, value.d2,
                value.a3, value.b3, value.c3, value.d3,
                value.a4, value.b4, value.c4, value.d4
            };
        }

        DirectX::XMFLOAT4 fallback_color(uint32_t material_id) {
            constexpr DirectX::XMFLOAT4 COLORS[] = {
                { 0.80f, 0.80f, 0.80f, 1.0f },
                { 0.82f, 0.48f, 0.32f, 1.0f },
                { 0.38f, 0.62f, 0.82f, 1.0f },
                { 0.48f, 0.75f, 0.42f, 1.0f }
            };
            return COLORS[material_id % std::size(COLORS)];
        }

        DirectX::XMFLOAT4 read_color(
            const aiMaterial* material,
            uint32_t material_id) {

            if (material == nullptr) return fallback_color(material_id);

            aiColor4D color{};
            if (aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &color) == AI_SUCCESS ||
                aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS) {
                return { color.r, color.g, color.b, color.a };
            }
            return fallback_color(material_id);
        }

        DirectX::XMFLOAT3 read_color3(
            const aiMaterial* material,
            const char* key,
            unsigned int type,
            unsigned int index) {

            if (material == nullptr) return {};

            aiColor3D color{};
            if (material->Get(key, type, index, color) == AI_SUCCESS) {
                return { color.r, color.g, color.b };
            }
            return {};
        }

        float read_float(
            const aiMaterial* material,
            const char* key,
            unsigned int type,
            unsigned int index,
            float fallback) {

            float value = fallback;
            if (material != nullptr &&
                material->Get(key, type, index, value) == AI_SUCCESS) {
                return value;
            }
            return fallback;
        }

        std::optional<TextureInput> read_texture(
            const aiMaterial* material,
            aiTextureType type) {

            if (material == nullptr || material->GetTextureCount(type) == 0) {
                return std::nullopt;
            }

            aiString path;
            unsigned int uv_set = 0;
            if (material->GetTexture(
                type,
                0,
                &path,
                nullptr,
                &uv_set) != AI_SUCCESS) {
                return std::nullopt;
            }

            TextureInput input{};
            input.path = path.C_Str();
            input.uv_set = uv_set;
            return input;
        }

        source::ImageFormat image_format(
            const std::filesystem::path& path) {

            std::string extension = path.extension().string();
            std::transform(
                extension.begin(),
                extension.end(),
                extension.begin(),
                [](unsigned char value) {
                    return static_cast<char>(std::tolower(value));
                });

            if (extension == ".jpg" || extension == ".jpeg") {
                return source::ImageFormat::Jpeg;
            }
            if (extension == ".png") return source::ImageFormat::Png;
            if (extension == ".webp") return source::ImageFormat::WebP;
            if (extension == ".ktx2") return source::ImageFormat::Ktx2;
            if (extension == ".dds") return source::ImageFormat::Dds;
            return source::ImageFormat::Unknown;
        }

        bool is_embedded(const std::filesystem::path& path) {
            const auto& native = path.native();
            return !native.empty() &&
                native.front() ==
                static_cast<std::filesystem::path::value_type>('*');
        }

        source::TextureRef add_texture(
            SceneSourceData& scene,
            std::unordered_map<std::string, uint32_t>& texture_ids,
            const std::filesystem::path& scene_directory,
            const std::optional<TextureInput>& input) {

            if (!input || input->path.empty() || is_embedded(input->path)) {
                return {};
            }

            std::filesystem::path path = input->path;
            if (path.is_relative()) path = scene_directory / path;
            path = std::filesystem::absolute(path).lexically_normal();

            const std::string key = path.generic_string();
            const auto found = texture_ids.find(key);
            uint32_t texture_id = source::SceneConstants::INVALID_INDEX;
            if (found != texture_ids.end()) {
                texture_id = found->second;
            } else {
                source::Image image{};
                image.path = path;
                image.format = image_format(path);

                source::Texture texture{};
                texture.image_id = static_cast<uint32_t>(scene.images.size());

                texture_id = static_cast<uint32_t>(scene.textures.size());
                scene.images.emplace_back(std::move(image));
                scene.textures.emplace_back(texture);
                texture_ids.emplace(key, texture_id);
            }

            source::TextureRef reference{};
            reference.texture_id = texture_id;
            reference.uv_set = input->uv_set;
            return reference;
        }

        source::Material build_material(
            SceneSourceData& scene,
            std::unordered_map<std::string, uint32_t>& texture_ids,
            const std::filesystem::path& scene_directory,
            const aiMaterial* input,
            uint32_t material_id) {

            source::Material material{};
            material.base_color = read_color(input, material_id);
            material.emissive_color = read_color3(
                input,
                AI_MATKEY_COLOR_EMISSIVE);
            material.metalness = read_float(
                input,
                AI_MATKEY_METALLIC_FACTOR,
                material.metalness);
            material.roughness = read_float(
                input,
                AI_MATKEY_ROUGHNESS_FACTOR,
                material.roughness);
            material.alpha_cutoff = read_float(
                input,
                AI_MATKEY_GLTF_ALPHACUTOFF,
                material.alpha_cutoff);

            aiString alpha_mode;
            if (input != nullptr &&
                input->Get(AI_MATKEY_GLTF_ALPHAMODE, alpha_mode) == AI_SUCCESS) {
                const std::string mode = alpha_mode.C_Str();
                if (mode == "MASK") material.alpha_mode = source::AlphaMode::Mask;
                if (mode == "BLEND") material.alpha_mode = source::AlphaMode::Blend;
            }

            int two_sided = 0;
            if (input != nullptr &&
                input->Get(AI_MATKEY_TWOSIDED, two_sided) == AI_SUCCESS) {
                material.double_sided = two_sided != 0;
            }

            std::optional<TextureInput> base_color =
                read_texture(input, aiTextureType_BASE_COLOR);
            if (!base_color) {
                base_color = read_texture(input, aiTextureType_DIFFUSE);
            }

            std::optional<TextureInput> metal_roughness =
                read_texture(input, aiTextureType_METALNESS);
            if (!metal_roughness) {
                metal_roughness =
                    read_texture(input, aiTextureType_DIFFUSE_ROUGHNESS);
            }
            if (!metal_roughness) {
                metal_roughness = read_texture(input, aiTextureType_SPECULAR);
            }

            std::optional<TextureInput> normal =
                read_texture(input, aiTextureType_NORMALS);
            if (!normal) normal = read_texture(input, aiTextureType_HEIGHT);

            std::optional<TextureInput> occlusion =
                read_texture(input, aiTextureType_AMBIENT_OCCLUSION);
            if (!occlusion) {
                occlusion = read_texture(input, aiTextureType_LIGHTMAP);
            }

            material.base_color_texture = add_texture(
                scene, texture_ids, scene_directory, base_color);
            material.metal_roughness_texture = add_texture(
                scene, texture_ids, scene_directory, metal_roughness);
            material.normal_texture = add_texture(
                scene, texture_ids, scene_directory, normal);
            material.emissive_texture = add_texture(
                scene,
                texture_ids,
                scene_directory,
                read_texture(input, aiTextureType_EMISSIVE));
            material.occlusion_texture = add_texture(
                scene, texture_ids, scene_directory, occlusion);
            return material;
        }

        bool build_mesh(
            source::Mesh& destination,
            const aiMesh* input,
            uint32_t material_count) {

            if (input == nullptr ||
                input->mNumVertices == 0 ||
                input->mNumFaces == 0) {
                return false;
            }

            source::Primitive primitive{};
            primitive.positions.reserve(input->mNumVertices);
            if (input->HasNormals()) {
                primitive.normals.reserve(input->mNumVertices);
            }
            if (input->HasTangentsAndBitangents()) {
                primitive.tangents.reserve(input->mNumVertices);
            }
            if (input->HasTextureCoords(0)) {
                primitive.uv0.reserve(input->mNumVertices);
            }

            for (uint32_t vertex_id = 0; vertex_id < input->mNumVertices; ++vertex_id) {
                const aiVector3D& position = input->mVertices[vertex_id];
                primitive.positions.push_back({
                    position.x,
                    position.y,
                    position.z
                });

                if (input->HasNormals()) {
                    const aiVector3D& normal = input->mNormals[vertex_id];
                    primitive.normals.push_back({
                        normal.x,
                        normal.y,
                        normal.z
                    });
                }

                if (input->HasTangentsAndBitangents()) {
                    const aiVector3D& normal = input->mNormals[vertex_id];
                    const aiVector3D& tangent = input->mTangents[vertex_id];
                    const aiVector3D cross = normal ^ tangent;
                    const float sign =
                        (cross * input->mBitangents[vertex_id]) > 0.0f
                        ? -1.0f
                        : 1.0f;
                    primitive.tangents.push_back({
                        tangent.x,
                        tangent.y,
                        tangent.z,
                        sign
                    });
                }

                if (input->HasTextureCoords(0)) {
                    const aiVector3D& uv = input->mTextureCoords[0][vertex_id];
                    primitive.uv0.push_back({ uv.x, uv.y });
                }
            }

            primitive.indices.reserve(
                static_cast<size_t>(input->mNumFaces) * 3);
            for (uint32_t face_id = 0; face_id < input->mNumFaces; ++face_id) {
                const aiFace& face = input->mFaces[face_id];
                if (face.mNumIndices != 3) continue;
                primitive.indices.push_back(face.mIndices[0]);
                primitive.indices.push_back(face.mIndices[1]);
                primitive.indices.push_back(face.mIndices[2]);
            }
            if (primitive.indices.empty()) return false;

            primitive.material_id =
                input->mMaterialIndex < material_count
                ? input->mMaterialIndex
                : 0;
            destination.primitives.emplace_back(std::move(primitive));
            return true;
        }

        uint32_t add_node(
            SceneSourceData& scene,
            uint32_t parent_id,
            const DirectX::XMFLOAT4X4& local_transform) {

            source::Node node{};
            node.local_transform = local_transform;

            const uint32_t node_id =
                static_cast<uint32_t>(scene.nodes.size());
            scene.nodes.emplace_back(std::move(node));
            if (parent_id == source::SceneConstants::INVALID_INDEX) {
                scene.root_node_id = node_id;
            } else {
                scene.nodes[parent_id].children.push_back(node_id);
            }
            return node_id;
        }

        void build_nodes(
            SceneSourceData& scene,
            const aiNode* input,
            uint32_t parent_id,
            const std::vector<uint32_t>& mesh_ids) {

            if (input == nullptr) return;

            const uint32_t node_id =
                add_node(scene, parent_id, convert_matrix(input->mTransformation));
            if (input->mNumMeshes == 1) {
                const uint32_t input_mesh_id = input->mMeshes[0];
                if (input_mesh_id < mesh_ids.size()) {
                    scene.nodes[node_id].mesh_id = mesh_ids[input_mesh_id];
                }
            } else {
                for (uint32_t mesh_id = 0; mesh_id < input->mNumMeshes; ++mesh_id) {
                    const uint32_t input_mesh_id = input->mMeshes[mesh_id];
                    if (input_mesh_id >= mesh_ids.size() ||
                        mesh_ids[input_mesh_id] ==
                        source::SceneConstants::INVALID_INDEX) {
                        continue;
                    }

                    DirectX::XMFLOAT4X4 identity{};
                    DirectX::XMStoreFloat4x4(
                        &identity,
                        DirectX::XMMatrixIdentity());
                    const uint32_t attachment_id =
                        add_node(scene, node_id, identity);
                    scene.nodes[attachment_id].mesh_id =
                        mesh_ids[input_mesh_id];
                }
            }

            for (uint32_t child_id = 0; child_id < input->mNumChildren; ++child_id) {
                build_nodes(
                    scene,
                    input->mChildren[child_id],
                    node_id,
                    mesh_ids);
            }
        }
    }

    std::unique_ptr<SceneSourceData> AssimpSceneSourceBuilder::build(
        const std::filesystem::path& path) {

        Assimp::Importer importer;
        const std::filesystem::path source_path =
            std::filesystem::absolute(path).lexically_normal();
        const unsigned int flags =
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_ImproveCacheLocality |
            aiProcess_ConvertToLeftHanded |
            aiProcess_SortByPType;

        const aiScene* input = importer.ReadFile(source_path.string(), flags);
        if (input == nullptr || input->mNumMeshes == 0) {
            util::Logger::g_logger <<
                "error building scene from assimp: " <<
                importer.GetErrorString();
            util::Logger::g_logger.assert_with_log(false);
            return nullptr;
        }

        auto scene = std::make_unique<SceneSourceData>();
        std::unordered_map<std::string, uint32_t> texture_ids;
        scene->materials.reserve(std::max(1u, input->mNumMaterials));
        for (uint32_t material_id = 0; material_id < input->mNumMaterials; ++material_id) {
            scene->materials.push_back(build_material(
                *scene,
                texture_ids,
                source_path.parent_path(),
                input->mMaterials[material_id],
                material_id));
        }
        if (scene->materials.empty()) {
            scene->materials.emplace_back();
        }

        std::vector<uint32_t> mesh_ids(
            input->mNumMeshes,
            source::SceneConstants::INVALID_INDEX);
        for (uint32_t input_mesh_id = 0; input_mesh_id < input->mNumMeshes; ++input_mesh_id) {
            source::Mesh mesh{};
            if (!build_mesh(
                mesh,
                input->mMeshes[input_mesh_id],
                static_cast<uint32_t>(scene->materials.size()))) {
                continue;
            }

            mesh_ids[input_mesh_id] =
                static_cast<uint32_t>(scene->meshes.size());
            scene->meshes.emplace_back(std::move(mesh));
        }

        util::Logger::g_logger.assert_with_log(!scene->meshes.empty(), "no triangle");

        build_nodes(
            *scene,
            input->mRootNode,
            source::SceneConstants::INVALID_INDEX,
            mesh_ids);
        if (scene->nodes.empty()) {
            DirectX::XMFLOAT4X4 identity{};
            DirectX::XMStoreFloat4x4(
                &identity,
                DirectX::XMMatrixIdentity());
            add_node(
                *scene,
                source::SceneConstants::INVALID_INDEX,
                identity);
        }

        SceneSourceDataValidator::validate(*scene);
        return std::move(scene);
    }
}
