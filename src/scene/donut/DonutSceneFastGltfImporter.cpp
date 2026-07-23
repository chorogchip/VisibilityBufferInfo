#include "scene/donut/DonutSceneFastGltfImporter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <fastgltf/core.hpp>
#include <fastgltf/dxmath_element_traits.hpp>
#include <fastgltf/tools.hpp>

#include "util/Logger.h"

namespace scene::donut {

    namespace {

        using MaterialTextureSlot = DonutSceneDataCPU::EnumMaterialTextureSlot;
        using TextureColorSpace = DonutSceneDataCPU::EnumTextureColorSpace;
        using TextureFallback = DonutSceneDataCPU::EnumTextureFallback;

        template <class... T>
        struct Overloaded : T... {
            using T::operator()...;
        };
        template <class... T>
        Overloaded(T...) -> Overloaded<T...>;

        struct TextureSource {
            std::filesystem::path path;
            bool embedded = false;
        };

        template <typename String>
        std::string to_std_string(const String& value) {
            return std::string(value.data(), value.size());
        }

        uint32_t to_uint32(size_t value, const char* message) {
            util::Logger::g_logger.assert_with_log(
                value <= (std::numeric_limits<uint32_t>::max)(), message);
            return static_cast<uint32_t>(value);
        }

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

        DirectX::XMMATRIX to_directx_matrix(const fastgltf::math::fmat4x4& value) {
            return DirectX::XMMATRIX(
                value[0][0], value[0][1], value[0][2], value[0][3],
                value[1][0], value[1][1], value[1][2], value[1][3],
                value[2][0], value[2][1], value[2][2], value[2][3],
                value[3][0], value[3][1], value[3][2], value[3][3]);
        }

        DirectX::XMMATRIX to_left_handed_matrix(DirectX::FXMMATRIX matrix) {
            const DirectX::XMMATRIX flip_z =
                DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);
            return DirectX::XMMatrixMultiply(
                flip_z,
                DirectX::XMMatrixMultiply(matrix, flip_z));
        }

        DirectX::XMMATRIX read_local_transform(const fastgltf::Node& node) {
            return to_left_handed_matrix(
                to_directx_matrix(fastgltf::getTransformMatrix(node)));
        }

        DirectX::XMFLOAT3 to_left_handed_vector(const DirectX::XMFLOAT3& value) {
            return { value.x, value.y, -value.z };
        }

        DirectX::XMFLOAT4 to_left_handed_tangent(const DirectX::XMFLOAT4& value) {
            return { value.x, value.y, -value.z, -value.w };
        }

        DirectX::XMFLOAT2 to_directx_uv(const DirectX::XMFLOAT2& value) {
            return { value.x, 1.0f - value.y };
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

        std::optional<size_t> texture_image_index(const fastgltf::Texture& texture) {
            if (texture.imageIndex) return *texture.imageIndex;
            if (texture.ddsImageIndex) return *texture.ddsImageIndex;
            if (texture.webpImageIndex) return *texture.webpImageIndex;
            if (texture.basisuImageIndex) return *texture.basisuImageIndex;
            return std::nullopt;
        }

        std::optional<TextureSource> texture_source(
            const fastgltf::Asset& asset,
            const fastgltf::TextureInfo& texture_info) {

            if (texture_info.textureIndex >= asset.textures.size()) {
                return std::nullopt;
            }

            const fastgltf::Texture& texture =
                asset.textures[texture_info.textureIndex];
            const std::optional<size_t> image_index =
                texture_image_index(texture);
            if (!image_index || *image_index >= asset.images.size()) {
                return std::nullopt;
            }

            const fastgltf::Image& image = asset.images[*image_index];
            return std::visit(Overloaded{
                [image_index](const fastgltf::sources::URI& source)
                    -> std::optional<TextureSource> {
                    if (source.uri.isDataUri()) {
                        return TextureSource{
                            std::filesystem::path(
                                "*fastgltf_embedded_image_" + std::to_string(*image_index)),
                            true
                        };
                    }
                    if (!source.uri.isLocalPath()) {
                        return std::nullopt;
                    }
                    return TextureSource{ source.uri.fspath(), false };
                },
                [image_index](const fastgltf::sources::BufferView&)
                    -> std::optional<TextureSource> {
                    return TextureSource{
                        std::filesystem::path(
                            "*fastgltf_embedded_image_" + std::to_string(*image_index)),
                        true
                    };
                },
                [image_index](const fastgltf::sources::Array&)
                    -> std::optional<TextureSource> {
                    return TextureSource{
                        std::filesystem::path(
                            "*fastgltf_embedded_image_" + std::to_string(*image_index)),
                        true
                    };
                },
                [image_index](const fastgltf::sources::Vector&)
                    -> std::optional<TextureSource> {
                    return TextureSource{
                        std::filesystem::path(
                            "*fastgltf_embedded_image_" + std::to_string(*image_index)),
                        true
                    };
                },
                [image_index](const fastgltf::sources::ByteView&)
                    -> std::optional<TextureSource> {
                    return TextureSource{
                        std::filesystem::path(
                            "*fastgltf_embedded_image_" + std::to_string(*image_index)),
                        true
                    };
                },
                [](const auto&) -> std::optional<TextureSource> {
                    return std::nullopt;
                }
                }, image.data);
        }

        uint32_t register_texture(
            DonutSceneDataCPU& scene,
            std::unordered_map<std::string, uint32_t>& texture_cache,
            const TextureSource& source,
            MaterialTextureSlot slot) {

            const TextureColorSpace color_space = color_space_for_slot(slot);
            const std::string cache_key = source.path.generic_string() +
                (color_space == DonutSceneDataCPU::EnumTextureColorSpace::SRGB ? "|srgb" : "|linear");
            const auto cached = texture_cache.find(cache_key);
            if (cached != texture_cache.end()) return cached->second;

            DonutSceneDataCPU::Texture texture{};
            texture.name = source.path.generic_string();
            texture.path = source.path;
            texture.color_space = color_space;
            texture.fallback = fallback_for_slot(slot);
            texture.embedded = source.embedded || is_embedded_texture_path(source.path);

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
            const std::optional<TextureSource>& source) {

            if (!source || source->path.empty()) return;

            material.texture_ids[static_cast<size_t>(slot)] =
                register_texture(scene, texture_cache, *source, slot);
        }

        DonutSceneDataCPU::EnumMaterialDomain material_domain(
            fastgltf::AlphaMode alpha_mode) {

            switch (alpha_mode) {
            case fastgltf::AlphaMode::Mask:
                return DonutSceneDataCPU::EnumMaterialDomain::AlphaTested;
            case fastgltf::AlphaMode::Blend:
                return DonutSceneDataCPU::EnumMaterialDomain::AlphaBlended;
            case fastgltf::AlphaMode::Opaque:
                break;
            }
            return DonutSceneDataCPU::EnumMaterialDomain::Opaque;
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
            const fastgltf::Asset& asset,
            const fastgltf::Material& source,
            uint32_t material_index) {

            DonutSceneDataCPU::Material material =
                make_default_material(material_index);

            if (!source.name.empty()) {
                material.name = to_std_string(source.name);
            }

            const fastgltf::math::nvec4& base_color =
                source.pbrData.baseColorFactor;
            material.base_color = {
                static_cast<float>(base_color.x()),
                static_cast<float>(base_color.y()),
                static_cast<float>(base_color.z()),
                std::clamp(static_cast<float>(base_color.w()), 0.0f, 1.0f)
            };
            material.emissive_color = {
                static_cast<float>(source.emissiveFactor.x() * source.emissiveStrength),
                static_cast<float>(source.emissiveFactor.y() * source.emissiveStrength),
                static_cast<float>(source.emissiveFactor.z() * source.emissiveStrength)
            };
            material.metalness =
                static_cast<float>(source.pbrData.metallicFactor);
            material.roughness =
                static_cast<float>(source.pbrData.roughnessFactor);
            material.alpha_cutoff =
                static_cast<float>(source.alphaCutoff);
            material.domain = material_domain(source.alphaMode);
            material.double_sided = source.doubleSided;

            if (source.normalTexture) {
                material.normal_scale =
                    static_cast<float>(source.normalTexture->scale);
            }
            if (source.occlusionTexture) {
                material.occlusion_strength =
                    static_cast<float>(source.occlusionTexture->strength);
            }

            set_material_texture(
                scene, texture_cache, material,
                DonutSceneDataCPU::EnumMaterialTextureSlot::BASE_COLOR,
                source.pbrData.baseColorTexture
                ? texture_source(asset, *source.pbrData.baseColorTexture)
                : std::nullopt);
            set_material_texture(
                scene, texture_cache, material,
                DonutSceneDataCPU::EnumMaterialTextureSlot::METAL_ROUGHNESS,
                source.pbrData.metallicRoughnessTexture
                ? texture_source(asset, *source.pbrData.metallicRoughnessTexture)
                : std::nullopt);
            set_material_texture(
                scene, texture_cache, material,
                DonutSceneDataCPU::EnumMaterialTextureSlot::NORMAL,
                source.normalTexture
                ? texture_source(asset, *source.normalTexture)
                : std::nullopt);
            set_material_texture(
                scene, texture_cache, material,
                DonutSceneDataCPU::EnumMaterialTextureSlot::EMISSIVE,
                source.emissiveTexture
                ? texture_source(asset, *source.emissiveTexture)
                : std::nullopt);
            set_material_texture(
                scene, texture_cache, material,
                DonutSceneDataCPU::EnumMaterialTextureSlot::OCCLUSION,
                source.occlusionTexture
                ? texture_source(asset, *source.occlusionTexture)
                : std::nullopt);

            if (source.transmission && source.transmission->transmissionTexture) {
                set_material_texture(
                    scene, texture_cache, material,
                    DonutSceneDataCPU::EnumMaterialTextureSlot::TRANSMISSION,
                    texture_source(asset, *source.transmission->transmissionTexture));
            }

            return material;
        }

        uint32_t ensure_default_material(
            DonutSceneDataCPU& scene,
            uint32_t& default_material_id) {

            if (default_material_id != DonutSceneDataCPU::INVALID_INDEX) {
                return default_material_id;
            }

            default_material_id = static_cast<uint32_t>(scene.materials.size());
            scene.materials.push_back(make_default_material(default_material_id));
            return default_material_id;
        }

        uint32_t material_id_for_primitive(
            DonutSceneDataCPU& scene,
            const fastgltf::Primitive& primitive,
            uint32_t& default_material_id) {

            if (primitive.materialIndex &&
                *primitive.materialIndex < scene.materials.size()) {
                return static_cast<uint32_t>(*primitive.materialIndex);
            }

            return ensure_default_material(scene, default_material_id);
        }

        bool append_primitive(
            DonutSceneDataCPU& scene,
            const fastgltf::Asset& asset,
            const fastgltf::Primitive& primitive,
            const std::string& name,
            uint32_t material_id) {

            if (primitive.type != fastgltf::PrimitiveType::Triangles) {
                return false;
            }

            const auto position_it = primitive.findAttribute("POSITION");
            if (position_it == primitive.attributes.end()) {
                return false;
            }
            if (!primitive.indicesAccessor) {
                return false;
            }

            const fastgltf::Accessor& position_accessor =
                asset.accessors[position_it->accessorIndex];
            const fastgltf::Accessor& index_accessor =
                asset.accessors[*primitive.indicesAccessor];
            if (position_accessor.count == 0 || index_accessor.count < 3) {
                return false;
            }

            const uint32_t vertex_offset = to_uint32(
                scene.positions.size(),
                "fastgltf vertex offset exceeds 32-bit addressing");
            const uint32_t index_offset = to_uint32(
                scene.indices.size(),
                "fastgltf index offset exceeds 32-bit addressing");
            const uint32_t vertex_count = to_uint32(
                position_accessor.count,
                "fastgltf primitive has too many vertices");

            scene.positions.resize(static_cast<size_t>(vertex_offset) + vertex_count);
            scene.prev_positions.resize(scene.positions.size());
            scene.texcoords.resize(scene.positions.size(), { 0.0f, 0.0f });
            scene.packed_normals.resize(
                scene.positions.size(),
                pack_snorm8(0.0f, 1.0f, 0.0f, 0.0f));
            scene.packed_tangents.resize(
                scene.positions.size(),
                pack_snorm8(1.0f, 0.0f, 0.0f, 1.0f));

            math::AABB submesh_aabb = math::AABB::create_empty();
            fastgltf::iterateAccessorWithIndex<DirectX::XMFLOAT3>(
                asset,
                position_accessor,
                [&](DirectX::XMFLOAT3 position, size_t index) {
                    const DirectX::XMFLOAT3 converted =
                        to_left_handed_vector(position);
                    const size_t destination_index =
                        static_cast<size_t>(vertex_offset) + index;
                    scene.positions[destination_index] = converted;
                    scene.prev_positions[destination_index] = converted;
                    submesh_aabb = submesh_aabb.get_union(
                        math::AABB::create_from_pos(converted));
                });

            const auto normal_it = primitive.findAttribute("NORMAL");
            if (normal_it != primitive.attributes.end()) {
                const fastgltf::Accessor& normal_accessor =
                    asset.accessors[normal_it->accessorIndex];
                fastgltf::iterateAccessorWithIndex<DirectX::XMFLOAT3>(
                    asset,
                    normal_accessor,
                    [&](DirectX::XMFLOAT3 normal, size_t index) {
                        if (index >= vertex_count) return;
                        normal = normalize3(to_left_handed_vector(normal));
                        scene.packed_normals[
                            static_cast<size_t>(vertex_offset) + index] =
                            pack_snorm8(normal.x, normal.y, normal.z, 0.0f);
                    });
            }

            const auto texcoord_it = primitive.findAttribute("TEXCOORD_0");
            if (texcoord_it != primitive.attributes.end()) {
                const fastgltf::Accessor& texcoord_accessor =
                    asset.accessors[texcoord_it->accessorIndex];
                fastgltf::iterateAccessorWithIndex<DirectX::XMFLOAT2>(
                    asset,
                    texcoord_accessor,
                    [&](DirectX::XMFLOAT2 texcoord, size_t index) {
                        if (index >= vertex_count) return;
                        scene.texcoords[
                            static_cast<size_t>(vertex_offset) + index] =
                            to_directx_uv(texcoord);
                    });
            }

            const auto tangent_it = primitive.findAttribute("TANGENT");
            if (tangent_it != primitive.attributes.end()) {
                const fastgltf::Accessor& tangent_accessor =
                    asset.accessors[tangent_it->accessorIndex];
                fastgltf::iterateAccessorWithIndex<DirectX::XMFLOAT4>(
                    asset,
                    tangent_accessor,
                    [&](DirectX::XMFLOAT4 tangent, size_t index) {
                        if (index >= vertex_count) return;
                        tangent = to_left_handed_tangent(tangent);
                        const DirectX::XMFLOAT3 tangent_xyz =
                            normalize3({ tangent.x, tangent.y, tangent.z });
                        scene.packed_tangents[
                            static_cast<size_t>(vertex_offset) + index] =
                            pack_snorm8(
                                tangent_xyz.x,
                                tangent_xyz.y,
                                tangent_xyz.z,
                                tangent.w >= 0.0f ? 1.0f : -1.0f);
                    });
            }

            std::vector<uint32_t> local_indices(index_accessor.count);
            fastgltf::copyFromAccessor<uint32_t>(
                asset,
                index_accessor,
                local_indices.data());

            for (size_t index = 0; index + 2 < local_indices.size(); index += 3) {
                const uint32_t i0 = local_indices[index + 0];
                const uint32_t i1 = local_indices[index + 1];
                const uint32_t i2 = local_indices[index + 2];
                if (i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) {
                    continue;
                }

                scene.indices.push_back(i0);
                scene.indices.push_back(i2);
                scene.indices.push_back(i1);
            }

            const uint32_t index_count =
                static_cast<uint32_t>(scene.indices.size()) - index_offset;
            if (index_count == 0) {
                scene.positions.resize(vertex_offset);
                scene.prev_positions.resize(vertex_offset);
                scene.texcoords.resize(vertex_offset);
                scene.packed_normals.resize(vertex_offset);
                scene.packed_tangents.resize(vertex_offset);
                scene.indices.resize(index_offset);
                return false;
            }

            DonutSceneDataCPU::Submesh submesh{};
            submesh.name = name;
            submesh.vertex_offset = vertex_offset;
            submesh.vertex_count = vertex_count;
            submesh.index_offset = index_offset;
            submesh.index_count = index_count;
            submesh.material_id = material_id;
            submesh.local_aabb = submesh_aabb;
            scene.submeshes.emplace_back(std::move(submesh));
            return true;
        }

        uint32_t append_mesh(
            DonutSceneDataCPU& scene,
            const fastgltf::Asset& asset,
            const fastgltf::Mesh& source_mesh,
            uint32_t source_mesh_id,
            uint32_t& default_material_id) {

            DonutSceneDataCPU::Mesh mesh{};
            mesh.name = source_mesh.name.empty()
                ? "mesh_" + std::to_string(source_mesh_id)
                : to_std_string(source_mesh.name);
            mesh.first_submesh = static_cast<uint32_t>(scene.submeshes.size());
            mesh.local_aabb = math::AABB::create_empty();

            for (size_t primitive_index = 0;
                primitive_index < source_mesh.primitives.size();
                ++primitive_index) {
                const fastgltf::Primitive& primitive =
                    source_mesh.primitives[primitive_index];
                const uint32_t material_id =
                    material_id_for_primitive(scene, primitive, default_material_id);
                const std::string submesh_name =
                    mesh.name + "_primitive_" + std::to_string(primitive_index);

                if (!append_primitive(
                    scene,
                    asset,
                    primitive,
                    submesh_name,
                    material_id)) {
                    continue;
                }

                ++mesh.submesh_count;
                mesh.local_aabb = mesh.local_aabb.get_union(
                    scene.submeshes.back().local_aabb);
            }

            if (mesh.submesh_count == 0) {
                return DonutSceneDataCPU::INVALID_INDEX;
            }

            const uint32_t mesh_id =
                static_cast<uint32_t>(scene.meshes.size());
            scene.meshes.emplace_back(std::move(mesh));
            return mesh_id;
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

            const uint32_t node_id =
                static_cast<uint32_t>(scene.nodes.size());
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

            node.subtree_world_aabb = result;
            return result;
        }

        void visit_node(
            DonutSceneDataCPU& scene,
            const fastgltf::Asset& asset,
            size_t source_node_id,
            uint32_t parent_id,
            DirectX::FXMMATRIX parent_world_transform,
            const std::vector<uint32_t>& mesh_ids) {

            if (source_node_id >= asset.nodes.size()) return;

            const fastgltf::Node& source_node = asset.nodes[source_node_id];
            const DirectX::XMMATRIX local_transform =
                read_local_transform(source_node);
            const DirectX::XMMATRIX world_transform =
                DirectX::XMMatrixMultiply(local_transform, parent_world_transform);

            const uint32_t node_id = append_node(
                scene,
                to_std_string(source_node.name),
                parent_id,
                local_transform,
                world_transform);

            if (source_node.meshIndex &&
                *source_node.meshIndex < mesh_ids.size() &&
                mesh_ids[*source_node.meshIndex] != DonutSceneDataCPU::INVALID_INDEX) {
                add_instance(scene, node_id, mesh_ids[*source_node.meshIndex]);
            }

            for (size_t child_id : source_node.children) {
                visit_node(
                    scene,
                    asset,
                    child_id,
                    node_id,
                    world_transform,
                    mesh_ids);
            }
        }

        std::optional<fastgltf::Asset> load_asset(
            const std::filesystem::path& path,
            std::string& error_message) {

            auto gltf_file = fastgltf::GltfDataBuffer::FromPath(path);
            if (!gltf_file) {
                error_message =
                    "fastgltf failed to open scene: " +
                    std::string(fastgltf::getErrorMessage(gltf_file.error()));
                return std::nullopt;
            }

            static constexpr auto SUPPORTED_EXTENSIONS =
                fastgltf::Extensions::KHR_accessor_float64 |
                fastgltf::Extensions::KHR_materials_clearcoat |
                fastgltf::Extensions::KHR_materials_diffuse_transmission |
                fastgltf::Extensions::KHR_materials_emissive_strength |
                fastgltf::Extensions::KHR_materials_ior |
                fastgltf::Extensions::KHR_materials_specular |
                fastgltf::Extensions::KHR_materials_transmission |
                fastgltf::Extensions::KHR_materials_unlit |
                fastgltf::Extensions::KHR_mesh_quantization |
                fastgltf::Extensions::KHR_texture_basisu |
                fastgltf::Extensions::KHR_texture_transform |
                fastgltf::Extensions::MSFT_texture_dds |
                fastgltf::Extensions::EXT_texture_webp;
            fastgltf::Parser parser(SUPPORTED_EXTENSIONS);

            constexpr auto OPTIONS =
                fastgltf::Options::DontRequireValidAssetMember |
                fastgltf::Options::AllowDouble |
                fastgltf::Options::LoadExternalBuffers |
                fastgltf::Options::GenerateMeshIndices;

            auto asset = parser.loadGltf(
                gltf_file.get(),
                path.parent_path(),
                OPTIONS,
                fastgltf::Category::OnlyRenderable);
            if (!asset) {
                error_message =
                    "fastgltf failed to parse scene: " +
                    std::string(fastgltf::getErrorMessage(asset.error()));
                return std::nullopt;
            }

            return std::move(asset.get());
        }
    }

    std::unique_ptr<DonutSceneDataCPU> DonutSceneFastGltfImporter::load(
        const std::filesystem::path& path) {

        auto result = std::make_unique<DonutSceneDataCPU>();
        result->source_path = std::filesystem::absolute(path).lexically_normal();

        std::string load_error;
        std::optional<fastgltf::Asset> asset =
            load_asset(result->source_path, load_error);
        if (!asset) {
            result->error_message = std::move(load_error);
            return result;
        }

        std::unordered_map<std::string, uint32_t> texture_cache;
        result->materials.reserve(asset->materials.size() + 1);
        for (uint32_t material_index = 0;
            material_index < asset->materials.size();
            ++material_index) {
            result->materials.push_back(import_material(
                *result,
                texture_cache,
                *asset,
                asset->materials[material_index],
                material_index));
        }

        uint32_t default_material_id = DonutSceneDataCPU::INVALID_INDEX;
        std::vector<uint32_t> mesh_ids(
            asset->meshes.size(),
            DonutSceneDataCPU::INVALID_INDEX);
        for (uint32_t mesh_index = 0; mesh_index < asset->meshes.size(); ++mesh_index) {
            mesh_ids[mesh_index] = append_mesh(
                *result,
                *asset,
                asset->meshes[mesh_index],
                mesh_index,
                default_material_id);
        }

        if (result->meshes.empty()) {
            result->error_message =
                "fastgltf scene had no triangle meshes after filtering.";
            return result;
        }

        const uint32_t root_node_id = append_node(
            *result,
            "root",
            DonutSceneDataCPU::INVALID_INDEX,
            DirectX::XMMatrixIdentity(),
            DirectX::XMMatrixIdentity());

        if (!asset->scenes.empty()) {
            const size_t scene_index =
                asset->defaultScene && *asset->defaultScene < asset->scenes.size()
                ? *asset->defaultScene
                : 0;
            for (size_t source_node_id : asset->scenes[scene_index].nodeIndices) {
                visit_node(
                    *result,
                    *asset,
                    source_node_id,
                    root_node_id,
                    DirectX::XMMatrixIdentity(),
                    mesh_ids);
            }
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
