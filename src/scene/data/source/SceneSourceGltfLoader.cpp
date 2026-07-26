#include "scene/data/source/SceneSourceGltfLoader.h"

#include <array>
#include <fstream>
#include <limits>
#include <optional>
#include <utility>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <simdjson.h>

#include "SceneSourceGltfInternal.h"
#include "util/Logger.h"

namespace scene::source::gltf {

    namespace {

        constexpr uint32_t GLB_MAGIC = 0x46546c67;
        constexpr uint32_t GLB_BINARY_CHUNK = 0x004e4942;

        bool read_glb_layout(
            const std::filesystem::path& path,
            FileLayout& layout) {

            if (path.extension() != ".glb") return true;

            std::ifstream input(path, std::ios::binary);
            if (!input) return false;

            uint32_t header[3]{};
            input.read(
                reinterpret_cast<char*>(header),
                sizeof(header));
            if (!input || header[0] != GLB_MAGIC || header[1] != 2) {
                return false;
            }

            while (input) {
                uint32_t chunk_header[2]{};
                input.read(
                    reinterpret_cast<char*>(chunk_header),
                    sizeof(chunk_header));
                if (!input) break;

                const uint64_t data_offset =
                    static_cast<uint64_t>(input.tellg());
                if (chunk_header[1] == GLB_BINARY_CHUNK) {
                    layout.binary_offset = data_offset;
                    layout.binary_size = chunk_header[0];
                    return true;
                }
                input.seekg(chunk_header[0], std::ios::cur);
            }
            return false;
        }

        bool parse_vec3(
            simdjson::dom::element element,
            DirectX::XMFLOAT3& result) {

            simdjson::dom::array values;
            if (element.get_array().get(values) != simdjson::SUCCESS ||
                values.size() != 3) {
                return false;
            }

            std::array<double, 3> decoded{};
            size_t index = 0;
            for (simdjson::dom::element value : values) {
                if (value.get_double().get(decoded[index]) !=
                    simdjson::SUCCESS) {
                    return false;
                }
                ++index;
            }

            // extras.jr bounds are authored in Blender Z-up coordinates.
            // glTF geometry becomes DirectX left-handed Y-up as (x, z, y).
            result = {
                static_cast<float>(decoded[0]),
                static_cast<float>(decoded[2]),
                static_cast<float>(decoded[1])
            };
            return true;
        }

        NodeKind parse_node_kind(std::string_view value) {
            if (value == "scene_root" || value == "root") {
                return NodeKind::SceneRoot;
            }
            if (value == "region") return NodeKind::Region;
            if (value == "cell") return NodeKind::Cell;
            if (value == "system") return NodeKind::System;
            if (value == "instance_set" ||
                value == "point_instancer") {
                return NodeKind::InstanceSet;
            }
            if (value == "static_object") {
                return NodeKind::StaticObject;
            }
            return NodeKind::Generic;
        }

        Region parse_region(std::string_view value) {
            if (value == "global") return Region::Global;
            if (value == "cinematic") return Region::Cinematic;
            if (value == "extended") return Region::Extended;
            if (value == "pyramid") return Region::Pyramid;
            return Region::None;
        }

        void extras_callback(
            simdjson::dom::object* extras,
            size_t object_index,
            fastgltf::Category category,
            void* user_pointer) {

            if (category != fastgltf::Category::Nodes) return;

            auto* context = static_cast<Context*>(user_pointer);
            context->node_metadata.resize(
                (std::max)(context->node_metadata.size(), object_index + 1));

            simdjson::dom::object jr;
            if ((*extras)["jr"].get_object().get(jr) !=
                simdjson::SUCCESS) {
                return;
            }

            NodeMetadata& metadata = context->node_metadata[object_index];
            std::string_view text;
            if (jr["entity_type"].get_string().get(text) ==
                simdjson::SUCCESS) {
                metadata.kind = parse_node_kind(text);
            }
            if (jr["region"].get_string().get(text) ==
                simdjson::SUCCESS) {
                metadata.region = parse_region(text);
            }
            if (jr["stable_id"].get_string().get(text) ==
                simdjson::SUCCESS) {
                metadata.stable_id.assign(text);
            }

            simdjson::dom::object bounds;
            if (jr["bounds"].get_object().get(bounds) ==
                simdjson::SUCCESS) {
                DirectX::XMFLOAT3 minimum{};
                DirectX::XMFLOAT3 maximum{};
                if (parse_vec3(bounds["min"], minimum) &&
                    parse_vec3(bounds["max"], maximum)) {
                    metadata.world_bounds.is_valid = true;
                    metadata.world_bounds.pos_min = minimum;
                    metadata.world_bounds.pos_max = maximum;
                }
            }
        }
    }

    uint32_t to_uint32(size_t value, const char* message) {
        util::Logger::g_logger.assert_with_log(
            value <= (std::numeric_limits<uint32_t>::max)(),
            message);
        return static_cast<uint32_t>(value);
    }

    DirectX::XMMATRIX read_local_transform(const fastgltf::Node& node) {
        const fastgltf::math::fmat4x4 value =
            fastgltf::getTransformMatrix(node);
        const DirectX::XMMATRIX matrix(
            value[0][0], value[0][1], value[0][2], value[0][3],
            value[1][0], value[1][1], value[1][2], value[1][3],
            value[2][0], value[2][1], value[2][2], value[2][3],
            value[3][0], value[3][1], value[3][2], value[3][3]);
        const DirectX::XMMATRIX flip_z =
            DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);
        return DirectX::XMMatrixMultiply(
            flip_z,
            DirectX::XMMatrixMultiply(matrix, flip_z));
    }

    std::optional<fastgltf::Asset> load_asset(
        Context& context,
        std::string& error_message) {

        if (!read_glb_layout(
            context.source_path,
            context.file_layout)) {
            error_message = "Invalid GLB container.";
            return std::nullopt;
        }

        auto gltf_file =
            fastgltf::GltfDataBuffer::FromPath(context.source_path);
        if (!gltf_file) {
            error_message =
                "fastgltf failed to open scene: " +
                std::string(
                    fastgltf::getErrorMessage(gltf_file.error()));
            return std::nullopt;
        }

        static constexpr auto SUPPORTED_EXTENSIONS =
            fastgltf::Extensions::EXT_mesh_gpu_instancing |
            fastgltf::Extensions::EXT_texture_webp |
            fastgltf::Extensions::KHR_materials_emissive_strength |
            fastgltf::Extensions::KHR_materials_ior |
            fastgltf::Extensions::KHR_materials_specular |
            fastgltf::Extensions::KHR_materials_transmission |
            fastgltf::Extensions::KHR_mesh_quantization |
            fastgltf::Extensions::KHR_texture_basisu |
            fastgltf::Extensions::KHR_texture_transform;
        fastgltf::Parser parser(SUPPORTED_EXTENSIONS);
        parser.setExtrasParseCallback(extras_callback);
        parser.setUserPointer(&context);

        constexpr auto OPTIONS =
            fastgltf::Options::DontRequireValidAssetMember |
            fastgltf::Options::LoadExternalBuffers |
            fastgltf::Options::GenerateMeshIndices;

        auto asset = parser.loadGltf(
            gltf_file.get(),
            context.source_path.parent_path(),
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

namespace scene {

    SceneSourceLoadResult SceneSourceGltfLoader::load(
        const std::filesystem::path& path) {

        SceneSourceLoadResult result{};
        source::gltf::Context context{};
        context.source_path =
            std::filesystem::absolute(path).lexically_normal();

        std::optional<fastgltf::Asset> asset =
            source::gltf::load_asset(context, result.error_message);
        if (!asset) return result;

        auto scene = std::make_unique<SceneSourceData>();
        std::vector<uint32_t> mesh_ids;
        source::gltf::append_cameras(*asset, *scene);
        if (!source::gltf::append_materials(
                context,
                *asset,
                *scene,
                result.error_message) ||
            !source::gltf::append_geometry(
                *asset,
                *scene,
                mesh_ids,
                result.error_message) ||
            !source::gltf::append_hierarchy(
                context,
                *asset,
                mesh_ids,
                *scene,
                result.error_message)) {
            return result;
        }

        scene->validate();
        result.scene = std::move(scene);
        result.error_message = "ok";
        return result;
    }
}
