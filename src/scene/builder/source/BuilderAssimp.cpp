#include "scene/builder/source/BuilderAssimp.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
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

        DirectX::XMFLOAT2 to_float2(const aiVector3D& value) {
            return { value.x, value.y };
        }

        DirectX::XMFLOAT3 to_float3(const aiVector3D& value) {
            return { value.x, value.y, value.z };
        }

        DirectX::XMFLOAT4X4 identity_matrix() {
            DirectX::XMFLOAT4X4 identity{};
            DirectX::XMStoreFloat4x4(
                &identity,
                DirectX::XMMatrixIdentity());
            return identity;
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

        std::optional<TextureInput> read_texture(
            const aiMaterial* material,
            std::initializer_list<aiTextureType> types) {

            for (const aiTextureType type : types) {
                if (std::optional<TextureInput> texture =
                    read_texture(material, type)) {
                    return texture;
                }
            }
            return std::nullopt;
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

        struct TextureRegistry {
            SceneSourceData& scene;
            const std::filesystem::path& scene_directory;
            std::unordered_map<std::string, uint32_t> ids;
        };

        source::TextureRef add_texture(
            TextureRegistry& registry,
            const std::optional<TextureInput>& input) {

            if (!input || input->path.empty() || is_embedded(input->path)) {
                return {};
            }

            std::filesystem::path path = input->path;
            if (path.is_relative()) path = registry.scene_directory / path;
            path = std::filesystem::absolute(path).lexically_normal();

            uint32_t texture_id = source::SceneConstants::INVALID_INDEX;
            const std::string key = path.generic_string();
            const auto found = registry.ids.find(key);
            if (found != registry.ids.end()) {
                texture_id = found->second;
            } else {
                source::Image image{};
                image.path = path;
                image.format = image_format(path);

                source::Texture texture{};
                texture.image_id =
                    static_cast<uint32_t>(registry.scene.images.size());

                texture_id =
                    static_cast<uint32_t>(registry.scene.textures.size());
                registry.scene.images.emplace_back(std::move(image));
                registry.scene.textures.emplace_back(texture);
                registry.ids.emplace(key, texture_id);
            }

            source::TextureRef reference{};
            reference.texture_id = texture_id;
            reference.uv_set = input->uv_set;
            return reference;
        }

        source::Material build_material(
            TextureRegistry& registry,
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

            material.base_color_texture = add_texture(registry, read_texture(
                input,
                { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }));
            material.metal_roughness_texture = add_texture(registry, read_texture(
                input,
                {
                    aiTextureType_METALNESS,
                    aiTextureType_DIFFUSE_ROUGHNESS,
                    aiTextureType_SPECULAR
                }));
            material.normal_texture = add_texture(registry, read_texture(
                input,
                { aiTextureType_NORMALS, aiTextureType_HEIGHT }));
            material.emissive_texture =
                add_texture(registry, read_texture(input, aiTextureType_EMISSIVE));
            material.occlusion_texture = add_texture(registry, read_texture(
                input,
                { aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP }));
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
            const bool has_normals = input->HasNormals();
            const bool has_tangents = input->HasTangentsAndBitangents();
            const bool has_uv0 = input->HasTextureCoords(0);

            primitive.positions.reserve(input->mNumVertices);
            if (has_normals) primitive.normals.reserve(input->mNumVertices);
            if (has_tangents) primitive.tangents.reserve(input->mNumVertices);
            if (has_uv0) primitive.uv0.reserve(input->mNumVertices);

            for (uint32_t vertex_id = 0; vertex_id < input->mNumVertices; ++vertex_id) {
                primitive.positions.push_back(
                    to_float3(input->mVertices[vertex_id]));

                if (has_normals) {
                    primitive.normals.push_back(
                        to_float3(input->mNormals[vertex_id]));
                }

                if (has_tangents) {
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

                if (has_uv0) {
                    primitive.uv0.push_back(
                        to_float2(input->mTextureCoords[0][vertex_id]));
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

        void build_materials(
            SceneSourceData& scene,
            const aiScene& input,
            const std::filesystem::path& scene_directory) {

            TextureRegistry textures{ scene, scene_directory };
            scene.materials.reserve(std::max(1u, input.mNumMaterials));
            for (uint32_t material_id = 0; material_id < input.mNumMaterials; ++material_id) {
                scene.materials.push_back(
                    build_material(textures, input.mMaterials[material_id], material_id));
            }
            if (scene.materials.empty()) scene.materials.emplace_back();
        }

        std::vector<uint32_t> build_meshes(
            SceneSourceData& scene,
            const aiScene& input) {

            std::vector<uint32_t> mesh_ids(
                input.mNumMeshes,
                source::SceneConstants::INVALID_INDEX);
            for (uint32_t input_mesh_id = 0; input_mesh_id < input.mNumMeshes; ++input_mesh_id) {
                source::Mesh mesh{};
                if (!build_mesh(
                    mesh,
                    input.mMeshes[input_mesh_id],
                    static_cast<uint32_t>(scene.materials.size()))) {
                    continue;
                }

                mesh_ids[input_mesh_id] =
                    static_cast<uint32_t>(scene.meshes.size());
                scene.meshes.emplace_back(std::move(mesh));
            }
            return mesh_ids;
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

        void build_nodes(SceneSourceData& scene, const aiNode* input, uint32_t parent_id, const std::vector<uint32_t>& mesh_ids) {

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

                    const uint32_t attachment_id =
                        add_node(scene, node_id, identity_matrix());
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

    std::unique_ptr<SceneSourceData> BuilderAssimp::build(const std::filesystem::path& path) {

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
        build_materials(*scene, *input, source_path.parent_path());
        const std::vector<uint32_t> mesh_ids = build_meshes(*scene, *input);

        util::Logger::g_logger.assert_with_log(!scene->meshes.empty(), "no triangle");

        build_nodes(*scene, input->mRootNode, source::SceneConstants::INVALID_INDEX, mesh_ids);

        if (scene->nodes.empty()) {
            add_node(
                *scene,
                source::SceneConstants::INVALID_INDEX,
                identity_matrix());
        }

        SceneSourceDataValidator::validate(*scene);
        return std::move(scene);
    }
}
