#include "JungleSceneSourceBuilderInternal.h"

#include <limits>
#include <string>
#include <vector>

#include <fastgltf/dxmath_element_traits.hpp>
#include <fastgltf/tools.hpp>

#include "util/Logger.h"

namespace scene::source::jungle {

    namespace {

        DirectX::XMFLOAT3 to_left_handed(
            const DirectX::XMFLOAT3& value) {
            return { value.x, value.y, -value.z };
        }

        DirectX::XMFLOAT4 to_left_handed_tangent(
            const DirectX::XMFLOAT4& value) {
            return { value.x, value.y, -value.z, -value.w };
        }

        uint32_t material_id_for_primitive(
            const fastgltf::Primitive& primitive,
            SceneSourceData& scene,
            uint32_t& default_material_id) {

            if (primitive.materialIndex &&
                *primitive.materialIndex < scene.materials.size()) {
                return to_uint32(
                    *primitive.materialIndex,
                    "glTF material index exceeds 32-bit indexing.");
            }

            if (default_material_id ==
                SceneConstants::INVALID_INDEX) {
                default_material_id =
                    to_uint32(
                        scene.materials.size(),
                        "Scene material count exceeds 32-bit indexing.");
                scene.materials.emplace_back();
            }
            return default_material_id;
        }

        void append_uv(
            const fastgltf::Asset& asset,
            const fastgltf::Primitive& source,
            const char* semantic,
            size_t vertex_count,
            std::vector<DirectX::XMFLOAT2>& destination) {

            const auto attribute = source.findAttribute(semantic);
            if (attribute == source.attributes.end()) return;

            util::Logger::g_logger.assert_with_log(
                attribute->accessorIndex < asset.accessors.size(),
                "glTF UV attribute references an invalid accessor.");
            const fastgltf::Accessor& accessor =
                asset.accessors[attribute->accessorIndex];
            util::Logger::g_logger.assert_with_log(
                accessor.count == vertex_count &&
                accessor.type == fastgltf::AccessorType::Vec2,
                "glTF UV attribute has an invalid layout.");
            destination.resize(vertex_count);
            fastgltf::iterateAccessorWithIndex<DirectX::XMFLOAT2>(
                asset,
                accessor,
                [&](DirectX::XMFLOAT2 value, size_t index) {
                    destination[index] = value;
                });
        }

        void append_color(
            const fastgltf::Asset& asset,
            const fastgltf::Primitive& source,
            const char* semantic,
            size_t vertex_count,
            std::vector<DirectX::XMFLOAT4>& destination) {

            const auto attribute = source.findAttribute(semantic);
            if (attribute == source.attributes.end()) return;

            util::Logger::g_logger.assert_with_log(
                attribute->accessorIndex < asset.accessors.size(),
                "glTF color attribute references an invalid accessor.");
            const fastgltf::Accessor& accessor =
                asset.accessors[attribute->accessorIndex];
            util::Logger::g_logger.assert_with_log(
                accessor.count == vertex_count,
                "glTF color count differs from its position count.");
            destination.resize(vertex_count);

            if (accessor.type == fastgltf::AccessorType::Vec3) {
                fastgltf::iterateAccessorWithIndex<
                    DirectX::XMFLOAT3>(
                    asset,
                    accessor,
                    [&](DirectX::XMFLOAT3 value, size_t index) {
                        destination[index] = {
                            value.x,
                            value.y,
                            value.z,
                            1.0f
                        };
                    });
                return;
            }
            if (accessor.type == fastgltf::AccessorType::Vec4) {
                fastgltf::iterateAccessorWithIndex<
                    DirectX::XMFLOAT4>(
                    asset,
                    accessor,
                    [&](DirectX::XMFLOAT4 value, size_t index) {
                        destination[index] = value;
                    });
                return;
            }
            util::Logger::g_logger.assert_with_log(
                false,
                "glTF color attribute must be VEC3 or VEC4.");
        }

        void append_primitive(
            const fastgltf::Asset& asset,
            const fastgltf::Primitive& source,
            uint32_t material_id,
            Primitive& primitive) {

            util::Logger::g_logger.assert_with_log(
                source.type == fastgltf::PrimitiveType::Triangles,
                "Jungle source builder requires triangle primitives.");

            const auto position_it =
                source.findAttribute("POSITION");
            util::Logger::g_logger.assert_with_log(
                position_it != source.attributes.end() &&
                source.indicesAccessor.has_value(),
                "Jungle primitive requires positions and indices.");
            util::Logger::g_logger.assert_with_log(
                position_it->accessorIndex < asset.accessors.size() &&
                *source.indicesAccessor < asset.accessors.size(),
                "Jungle primitive references an invalid accessor.");

            const fastgltf::Accessor& position_accessor =
                asset.accessors[position_it->accessorIndex];
            const fastgltf::Accessor& index_accessor =
                asset.accessors[*source.indicesAccessor];
            util::Logger::g_logger.assert_with_log(
                position_accessor.count != 0 &&
                index_accessor.count >= 3,
                "Jungle primitive has no indexed triangles.");

            primitive.material_id = material_id;
            primitive.positions.resize(position_accessor.count);
            fastgltf::iterateAccessorWithIndex<DirectX::XMFLOAT3>(
                asset,
                position_accessor,
                [&](DirectX::XMFLOAT3 value, size_t index) {
                    primitive.positions[index] =
                        to_left_handed(value);
                });

            const auto normal_it =
                source.findAttribute("NORMAL");
            if (normal_it != source.attributes.end()) {
                util::Logger::g_logger.assert_with_log(
                    normal_it->accessorIndex < asset.accessors.size(),
                    "glTF normal references an invalid accessor.");
                const fastgltf::Accessor& accessor =
                    asset.accessors[normal_it->accessorIndex];
                util::Logger::g_logger.assert_with_log(
                    accessor.count == position_accessor.count,
                    "glTF normal count differs from its position count.");
                primitive.normals.resize(position_accessor.count);
                fastgltf::iterateAccessorWithIndex<
                    DirectX::XMFLOAT3>(
                    asset,
                    accessor,
                    [&](DirectX::XMFLOAT3 value, size_t index) {
                        if (index < primitive.normals.size()) {
                            primitive.normals[index] =
                                to_left_handed(value);
                        }
                    });
            }

            const auto tangent_it =
                source.findAttribute("TANGENT");
            if (tangent_it != source.attributes.end()) {
                util::Logger::g_logger.assert_with_log(
                    tangent_it->accessorIndex < asset.accessors.size(),
                    "glTF tangent references an invalid accessor.");
                const fastgltf::Accessor& accessor =
                    asset.accessors[tangent_it->accessorIndex];
                util::Logger::g_logger.assert_with_log(
                    accessor.count == position_accessor.count,
                    "glTF tangent count differs from its position count.");
                primitive.tangents.resize(position_accessor.count);
                fastgltf::iterateAccessorWithIndex<
                    DirectX::XMFLOAT4>(
                    asset,
                    accessor,
                    [&](DirectX::XMFLOAT4 value, size_t index) {
                        if (index < primitive.tangents.size()) {
                            primitive.tangents[index] =
                                to_left_handed_tangent(value);
                        }
                    });
            }

            append_uv(
                asset,
                source,
                "TEXCOORD_0",
                position_accessor.count,
                primitive.uv0);
            append_uv(
                asset,
                source,
                "TEXCOORD_1",
                position_accessor.count,
                primitive.uv1);
            append_color(
                asset,
                source,
                "COLOR_0",
                position_accessor.count,
                primitive.color0);
            append_color(
                asset,
                source,
                "COLOR_1",
                position_accessor.count,
                primitive.color1);

            std::vector<uint32_t> source_indices(
                index_accessor.count);
            fastgltf::copyFromAccessor<uint32_t>(
                asset,
                index_accessor,
                source_indices.data());
            primitive.indices.reserve(
                source_indices.size() -
                source_indices.size() % 3);
            for (size_t index = 0; index + 2 < source_indices.size(); index += 3) {
                const uint32_t i0 = source_indices[index + 0];
                const uint32_t i1 = source_indices[index + 1];
                const uint32_t i2 = source_indices[index + 2];
                util::Logger::g_logger.assert_with_log(
                    i0 < primitive.positions.size() &&
                    i1 < primitive.positions.size() &&
                    i2 < primitive.positions.size(),
                    "Jungle primitive contains an invalid vertex index.");
                primitive.indices.push_back(i0);
                primitive.indices.push_back(i2);
                primitive.indices.push_back(i1);
            }
            util::Logger::g_logger.assert_with_log(
                !primitive.indices.empty(),
                "Jungle primitive has no valid indexed triangles.");
        }
    }

    void append_geometry(
        const fastgltf::Asset& asset,
        SceneSourceData& scene,
        std::vector<uint32_t>& mesh_ids) {

        mesh_ids.assign(
            asset.meshes.size(),
            SceneConstants::INVALID_INDEX);
        scene.meshes.reserve(asset.meshes.size());
        uint32_t default_material_id =
            SceneConstants::INVALID_INDEX;

        for (size_t source_mesh_id = 0; source_mesh_id < asset.meshes.size(); ++source_mesh_id) {
            const fastgltf::Mesh& source =
                asset.meshes[source_mesh_id];
            Mesh mesh{};
            mesh.name.assign(source.name.data(), source.name.size());
            mesh.primitives.reserve(source.primitives.size());

            for (const fastgltf::Primitive& source_primitive : source.primitives) {
                Primitive primitive{};
                const uint32_t material_id =
                    material_id_for_primitive(
                        source_primitive,
                        scene,
                        default_material_id);
                append_primitive(
                    asset,
                    source_primitive,
                    material_id,
                    primitive);
                mesh.primitives.emplace_back(
                    std::move(primitive));
            }

            if (mesh.primitives.empty()) continue;

            mesh_ids[source_mesh_id] = to_uint32(
                scene.meshes.size(),
                "Scene mesh count exceeds 32-bit indexing.");
            scene.meshes.emplace_back(std::move(mesh));
        }

        util::Logger::g_logger.assert_with_log(
            !scene.meshes.empty(),
            "Jungle scene contains no indexed triangle meshes.");
    }
}
