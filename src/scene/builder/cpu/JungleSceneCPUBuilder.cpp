#include "scene/builder/cpu/JungleSceneCPUBuilder.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <DirectXMath.h>

#include "scene/builder/cpu/SceneCPUBuilder.h"
#include "scene/data/source/SceneConstants.h"
#include "util/Logger.h"

namespace scene {

    namespace {

        constexpr uint32_t INVALID_INDEX =
            source::SceneConstants::INVALID_INDEX;

        void append_diagnostic(
            SceneSourceData& scene,
            source::ConversionSeverity severity,
            const source::SourceReference& reference,
            std::string code,
            std::string message) {

            scene.conversion_diagnostics.push_back({
                severity,
                reference,
                std::move(code),
                std::move(message),
            });
        }

        [[noreturn]] void fail_materialization(
            SceneSourceData& scene,
            const source::SourceReference& reference,
            std::string code,
            std::string message) {

            append_diagnostic(
                scene,
                source::ConversionSeverity::Fatal,
                reference,
                std::move(code),
                message);
            util::Logger::g_logger <<
                "Jungle materialization fatal: " << message << '\n';
            throw std::runtime_error(message);
        }

        template<typename T>
        void append_vertex_attribute(
            std::vector<T>& destination,
            const std::vector<T>& source,
            size_t existing_vertex_count,
            size_t appended_vertex_count,
            const T& fallback) {

            if (destination.empty() && source.empty()) {
                return;
            }
            if (destination.empty()) {
                destination.resize(
                    existing_vertex_count,
                    fallback);
            }
            if (source.empty()) {
                destination.insert(
                    destination.end(),
                    appended_vertex_count,
                    fallback);
            }
            else {
                destination.insert(
                    destination.end(),
                    source.begin(),
                    source.end());
            }
        }

        void compact_primitives_by_material(
            SceneSourceData& semantic,
            source::Mesh& mesh,
            const source::SourceReference& reference) {

            if (mesh.primitives.size() < 2) {
                return;
            }

            const size_t original_count = mesh.primitives.size();
            std::vector<source::Primitive> compacted;
            compacted.reserve(original_count);
            std::unordered_map<uint32_t, size_t> primitive_by_material;

            for (source::Primitive& primitive : mesh.primitives) {
                const auto found =
                    primitive_by_material.find(
                        primitive.material_id);
                if (found == primitive_by_material.end()) {
                    primitive_by_material.emplace(
                        primitive.material_id,
                        compacted.size());
                    compacted.push_back(std::move(primitive));
                    continue;
                }

                source::Primitive& destination =
                    compacted[found->second];
                const size_t vertex_offset =
                    destination.positions.size();
                if (vertex_offset >
                    (std::numeric_limits<uint32_t>::max)() ||
                    primitive.positions.size() >
                    (std::numeric_limits<uint32_t>::max)() -
                        vertex_offset) {
                    fail_materialization(
                        semantic,
                        reference,
                        "prototype_vertex_32bit_overflow",
                        "Compacting a Jungle prototype by material exceeds "
                        "the legacy 32-bit vertex contract.");
                }

                const size_t appended_vertex_count =
                    primitive.positions.size();
                append_vertex_attribute(
                    destination.normals,
                    primitive.normals,
                    vertex_offset,
                    appended_vertex_count,
                    DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f });
                append_vertex_attribute(
                    destination.tangents,
                    primitive.tangents,
                    vertex_offset,
                    appended_vertex_count,
                    DirectX::XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f });
                append_vertex_attribute(
                    destination.uv0,
                    primitive.uv0,
                    vertex_offset,
                    appended_vertex_count,
                    DirectX::XMFLOAT2{ 0.0f, 0.0f });
                append_vertex_attribute(
                    destination.uv1,
                    primitive.uv1,
                    vertex_offset,
                    appended_vertex_count,
                    DirectX::XMFLOAT2{ 0.0f, 0.0f });
                append_vertex_attribute(
                    destination.color0,
                    primitive.color0,
                    vertex_offset,
                    appended_vertex_count,
                    DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });
                append_vertex_attribute(
                    destination.color1,
                    primitive.color1,
                    vertex_offset,
                    appended_vertex_count,
                    DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });
                destination.positions.insert(
                    destination.positions.end(),
                    primitive.positions.begin(),
                    primitive.positions.end());
                destination.indices.reserve(
                    destination.indices.size() +
                    primitive.indices.size());
                for (const uint32_t index : primitive.indices) {
                    destination.indices.push_back(
                        index +
                        static_cast<uint32_t>(vertex_offset));
                }
            }

            mesh.primitives = std::move(compacted);
            if (mesh.primitives.size() != original_count) {
                append_diagnostic(
                    semantic,
                    source::ConversionSeverity::Info,
                    reference,
                    "prototype_submesh_compaction",
                    "Legacy Jungle prototype primitives sharing a material "
                    "were compacted from " +
                    std::to_string(original_count) + " to " +
                    std::to_string(mesh.primitives.size()) +
                    " submeshes before instance expansion.");
            }
        }

        std::string lower_copy(std::string value) {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                });
            return value;
        }

        bool matches_name(
            const std::string& value,
            std::initializer_list<std::string_view> names) {

            const std::string lower = lower_copy(value);
            for (const std::string_view name : names) {
                if (lower == name) {
                    return true;
                }
            }
            return false;
        }

        std::vector<float> parse_floats(const std::string& text) {
            std::vector<float> result;
            const char* cursor = text.c_str();
            const char* const end = cursor + text.size();
            while (cursor < end) {
                char* parsed_end = nullptr;
                const float value = std::strtof(cursor, &parsed_end);
                if (parsed_end != cursor) {
                    result.push_back(value);
                    cursor = parsed_end;
                }
                else {
                    ++cursor;
                }
            }
            return result;
        }

        const source::ShaderValue* find_input(
            const source::ShaderNode& shader,
            std::initializer_list<std::string_view> names) {

            for (const source::ShaderValue& input : shader.inputs) {
                if (matches_name(input.name, names)) {
                    return &input;
                }
            }
            return nullptr;
        }

        const source::ShaderNode* find_shader_for_property(
            const SceneSourceData& scene,
            const source::MaterialGraph& graph,
            const std::string& property_path) {

            for (const uint32_t shader_id : graph.shader_node_ids) {
                if (shader_id >= scene.shader_nodes.size()) {
                    continue;
                }
                const source::ShaderNode& shader =
                    scene.shader_nodes[shader_id];
                const std::string prefix =
                    shader.source.prim_path + ".";
                if (property_path.starts_with(prefix)) {
                    return &shader;
                }
            }
            return nullptr;
        }

        source::ImageFormat image_format(
            const std::filesystem::path& path) {

            const std::string extension =
                lower_copy(path.extension().string());
            if (extension == ".jpg" || extension == ".jpeg") {
                return source::ImageFormat::Jpeg;
            }
            if (extension == ".png") return source::ImageFormat::Png;
            if (extension == ".webp") return source::ImageFormat::WebP;
            if (extension == ".ktx2") return source::ImageFormat::Ktx2;
            if (extension == ".dds") return source::ImageFormat::Dds;
            return source::ImageFormat::Unknown;
        }

        struct TextureRegistry {
            SceneSourceData& semantic;
            SceneSourceData& legacy;
            std::unordered_map<std::string, uint32_t> texture_ids;

            source::TextureRef add(
                const source::ShaderValue& asset_value,
                const source::SourceReference& material_reference) {

                if (asset_value.resolved_asset_path.empty()) {
                    if (!asset_value.authored_asset_path.empty()) {
                        append_diagnostic(
                            semantic,
                            source::ConversionSeverity::Warning,
                            asset_value.source,
                            "unresolved_material_texture",
                            "PBR texture keeps its authored path in the "
                            "semantic graph but has no resolved GPU path.");
                    }
                    return {};
                }

                const std::filesystem::path path =
                    std::filesystem::path(
                        asset_value.resolved_asset_path).lexically_normal();
                std::error_code error;
                if (!path.is_absolute() ||
                    !std::filesystem::is_regular_file(path, error) ||
                    error) {
                    append_diagnostic(
                        semantic,
                        source::ConversionSeverity::Warning,
                        material_reference,
                        "invalid_resolved_material_texture",
                        "Only an existing absolute resolved texture path is "
                        "eligible for the legacy GPU material.");
                    return {};
                }

                const std::string key = path.generic_string();
                const auto found = texture_ids.find(key);
                uint32_t texture_id = INVALID_INDEX;
                if (found != texture_ids.end()) {
                    texture_id = found->second;
                }
                else {
                    source::Image image{};
                    image.path = path;
                    image.format = image_format(path);
                    source::Texture texture{};
                    texture.image_id =
                        static_cast<uint32_t>(legacy.images.size());
                    texture_id =
                        static_cast<uint32_t>(legacy.textures.size());
                    legacy.images.push_back(std::move(image));
                    legacy.textures.push_back(texture);
                    texture_ids.emplace(key, texture_id);
                }

                source::TextureRef result{};
                result.texture_id = texture_id;
                result.uv_set = 0;
                return result;
            }
        };

        const source::ShaderValue* connected_texture_asset(
            const SceneSourceData& scene,
            const source::MaterialGraph& graph,
            const source::ShaderValue* destination) {

            if (destination == nullptr) {
                return nullptr;
            }
            for (const source::ShaderConnection& connection :
                graph.connections) {
                if (connection.destination_property_path !=
                    destination->source.property_path) {
                    continue;
                }
                const source::ShaderNode* source_shader =
                    find_shader_for_property(
                        scene,
                        graph,
                        connection.source_property_path);
                if (source_shader == nullptr) {
                    continue;
                }
                if (const source::ShaderValue* file = find_input(
                    *source_shader,
                    { "file", "filename", "tex" })) {
                    return file;
                }
            }
            return nullptr;
        }

        void apply_scalar(
            float& destination,
            const source::ShaderValue* input) {

            if (input == nullptr) return;
            const std::vector<float> values =
                parse_floats(input->authored_value);
            if (!values.empty()) destination = values.front();
        }

        void apply_color3(
            DirectX::XMFLOAT3& destination,
            const source::ShaderValue* input) {

            if (input == nullptr) return;
            const std::vector<float> values =
                parse_floats(input->authored_value);
            if (values.size() >= 3) {
                destination = { values[0], values[1], values[2] };
            }
        }

        void apply_color4(
            DirectX::XMFLOAT4& destination,
            const source::ShaderValue* input) {

            if (input == nullptr) return;
            const std::vector<float> values =
                parse_floats(input->authored_value);
            if (values.size() >= 3) {
                destination.x = values[0];
                destination.y = values[1];
                destination.z = values[2];
                if (values.size() >= 4) destination.w = values[3];
            }
        }

        std::unordered_map<std::string, uint32_t> derive_materials(
            SceneSourceData& semantic,
            SceneSourceData& legacy) {

            legacy.materials.emplace_back();
            legacy.materials.front().name = "Jungle fallback material";
            std::unordered_map<std::string, uint32_t> material_ids;
            TextureRegistry textures{ semantic, legacy };

            for (const source::MaterialGraph& graph :
                semantic.material_graphs) {
                const source::ShaderNode* pbr_shader = nullptr;
                for (const uint32_t shader_id : graph.shader_node_ids) {
                    if (shader_id >= semantic.shader_nodes.size()) {
                        continue;
                    }
                    const source::ShaderNode& shader =
                        semantic.shader_nodes[shader_id];
                    const std::string shader_name =
                        lower_copy(shader.shader_id);
                    if (shader_name.find("usdpreviewsurface") !=
                            std::string::npos ||
                        shader_name.find("standard_surface") !=
                            std::string::npos) {
                        pbr_shader = &shader;
                        break;
                    }
                }

                source::Material material{};
                material.name = graph.source.prim_path;
                if (pbr_shader == nullptr) {
                    append_diagnostic(
                        semantic,
                        source::ConversionSeverity::Info,
                        graph.source,
                        "unsupported_pbr_material_graph",
                        "The generic shader graph is preserved; the legacy "
                        "renderer uses fallback PBR values because no "
                        "UsdPreviewSurface/standard_surface node was found.");
                }
                else {
                    const source::ShaderValue* base_color = find_input(
                        *pbr_shader,
                        { "diffusecolor", "base_color", "basecolor" });
                    const source::ShaderValue* emissive = find_input(
                        *pbr_shader,
                        { "emissivecolor", "emission_color" });
                    const source::ShaderValue* metalness = find_input(
                        *pbr_shader,
                        { "metallic", "metalness" });
                    const source::ShaderValue* roughness = find_input(
                        *pbr_shader,
                        { "roughness", "specular_roughness" });
                    const source::ShaderValue* opacity = find_input(
                        *pbr_shader,
                        { "opacity" });
                    const source::ShaderValue* opacity_threshold = find_input(
                        *pbr_shader,
                        { "opacitythreshold", "opacity_threshold" });
                    const source::ShaderValue* normal = find_input(
                        *pbr_shader,
                        { "normal" });
                    const source::ShaderValue* occlusion = find_input(
                        *pbr_shader,
                        { "occlusion" });

                    apply_color4(material.base_color, base_color);
                    apply_color3(material.emissive_color, emissive);
                    apply_scalar(material.metalness, metalness);
                    apply_scalar(material.roughness, roughness);
                    apply_scalar(material.base_color.w, opacity);
                    apply_scalar(material.alpha_cutoff, opacity_threshold);
                    if (opacity_threshold != nullptr &&
                        material.alpha_cutoff > 0.0f) {
                        material.alpha_mode = source::AlphaMode::Mask;
                    }
                    else if (material.base_color.w < 1.0f) {
                        material.alpha_mode = source::AlphaMode::Blend;
                    }

                    if (const source::ShaderValue* asset =
                        connected_texture_asset(
                            semantic,
                            graph,
                            base_color)) {
                        material.base_color_texture =
                            textures.add(*asset, graph.source);
                    }
                    const source::ShaderValue* metallic_asset =
                        connected_texture_asset(
                            semantic,
                            graph,
                            metalness);
                    const source::ShaderValue* roughness_asset =
                        connected_texture_asset(
                            semantic,
                            graph,
                            roughness);
                    if (metallic_asset != nullptr &&
                        roughness_asset != nullptr &&
                        metallic_asset->resolved_asset_path ==
                            roughness_asset->resolved_asset_path) {
                        material.metal_roughness_texture =
                            textures.add(
                                *metallic_asset,
                                graph.source);
                    }
                    else if (metallic_asset != nullptr ||
                        roughness_asset != nullptr) {
                        append_diagnostic(
                            semantic,
                            source::ConversionSeverity::Info,
                            graph.source,
                            "separate_metal_roughness_textures",
                            "The semantic graph preserves separate metalness/"
                            "roughness textures; the legacy renderer requires "
                            "one packed texture and therefore uses scalar "
                            "factors.");
                    }
                    if (const source::ShaderValue* asset =
                        connected_texture_asset(
                            semantic,
                            graph,
                            normal)) {
                        material.normal_texture =
                            textures.add(*asset, graph.source);
                    }
                    if (const source::ShaderValue* asset =
                        connected_texture_asset(
                            semantic,
                            graph,
                            emissive)) {
                        material.emissive_texture =
                            textures.add(*asset, graph.source);
                    }
                    if (const source::ShaderValue* asset =
                        connected_texture_asset(
                            semantic,
                            graph,
                            occlusion)) {
                        material.occlusion_texture =
                            textures.add(*asset, graph.source);
                    }

                    append_diagnostic(
                        semantic,
                        source::ConversionSeverity::Info,
                        graph.source,
                        "legacy_pbr_subset",
                        "A renderer-compatible PBR subset was derived; the "
                        "complete generic shader graph remains in semantic "
                        "SceneSourceData.");
                }

                const uint32_t material_id =
                    static_cast<uint32_t>(legacy.materials.size());
                legacy.materials.push_back(std::move(material));
                material_ids.emplace(graph.source.stable_id, material_id);
            }
            return material_ids;
        }

        uint64_t primvar_value_count(const source::Primvar& primvar) {
            if (primvar.numeric_component_count == 0) return 0;
            const uint64_t components = primvar.numeric_component_count;
            if (!primvar.float_values.empty()) {
                return primvar.float_values.size() / components;
            }
            if (!primvar.double_values.empty()) {
                return primvar.double_values.size() / components;
            }
            if (!primvar.integer_values.empty()) {
                return primvar.integer_values.size() / components;
            }
            return 0;
        }

        bool primvar_component(
            const source::Primvar& primvar,
            uint32_t value_index,
            uint32_t component,
            float& result) {

            const uint64_t offset =
                static_cast<uint64_t>(value_index) *
                    primvar.numeric_component_count +
                component;
            if (component >= primvar.numeric_component_count) return false;
            if (offset < primvar.float_values.size()) {
                result = primvar.float_values[static_cast<size_t>(offset)];
                return true;
            }
            if (offset < primvar.double_values.size()) {
                result = static_cast<float>(
                    primvar.double_values[static_cast<size_t>(offset)]);
                return true;
            }
            if (offset < primvar.integer_values.size()) {
                result = static_cast<float>(
                    primvar.integer_values[static_cast<size_t>(offset)]);
                return true;
            }
            return false;
        }

        std::optional<uint32_t> primvar_value_index(
            const source::Primvar& primvar,
            uint32_t face_id,
            uint32_t face_vertex_id,
            uint32_t point_id) {

            uint64_t logical_index = 0;
            switch (primvar.interpolation) {
            case source::PrimvarInterpolation::Constant:
                logical_index = 0;
                break;
            case source::PrimvarInterpolation::Uniform:
                logical_index = face_id;
                break;
            case source::PrimvarInterpolation::Vertex:
            case source::PrimvarInterpolation::Varying:
                logical_index = point_id;
                break;
            case source::PrimvarInterpolation::FaceVarying:
                logical_index = face_vertex_id;
                break;
            default:
                return std::nullopt;
            }

            if (primvar.indexed) {
                if (logical_index >= primvar.indices.size()) {
                    return std::nullopt;
                }
                logical_index =
                    primvar.indices[static_cast<size_t>(logical_index)];
            }
            if (logical_index >= primvar_value_count(primvar) ||
                logical_index > (std::numeric_limits<uint32_t>::max)()) {
                return std::nullopt;
            }
            return static_cast<uint32_t>(logical_index);
        }

        struct SupportedPrimvars {
            const source::Primvar* normal = nullptr;
            const source::Primvar* tangent = nullptr;
            const source::Primvar* uv0 = nullptr;
        };

        SupportedPrimvars classify_primvars(
            SceneSourceData& semantic,
            const source::PolygonMesh& mesh) {

            SupportedPrimvars result{};
            for (const source::Primvar& primvar : mesh.primvars) {
                if (matches_name(
                    primvar.name,
                    { "normals", "normal" })) {
                    if (primvar.numeric_component_count >= 3 &&
                        result.normal == nullptr) {
                        result.normal = &primvar;
                    }
                    else {
                        append_diagnostic(
                            semantic,
                            source::ConversionSeverity::Warning,
                            primvar.source,
                            "unsupported_normal_primvar",
                            "Normal primvar must have at least three numeric "
                            "components; flat normals will be generated.");
                    }
                }
                else if (matches_name(
                    primvar.name,
                    { "tangents", "tangent" })) {
                    if (primvar.numeric_component_count >= 3 &&
                        result.tangent == nullptr) {
                        result.tangent = &primvar;
                    }
                    else {
                        append_diagnostic(
                            semantic,
                            source::ConversionSeverity::Warning,
                            primvar.source,
                            "unsupported_tangent_primvar",
                            "Tangent primvar must have at least three numeric "
                            "components; the legacy tangent default is used.");
                    }
                }
                else if (matches_name(
                    primvar.name,
                    { "st", "uv", "uv0", "texcoord0" })) {
                    if (primvar.numeric_component_count >= 2 &&
                        result.uv0 == nullptr) {
                        result.uv0 = &primvar;
                    }
                    else {
                        append_diagnostic(
                            semantic,
                            source::ConversionSeverity::Warning,
                            primvar.source,
                            "unsupported_uv_primvar",
                            "UV0 primvar must have at least two numeric "
                            "components.");
                    }
                }
                else {
                    append_diagnostic(
                        semantic,
                        source::ConversionSeverity::Info,
                        primvar.source,
                        "unmaterialized_primvar",
                        "The primvar remains in semantic SceneSourceData but "
                        "is outside the legacy normal/tangent/UV0 subset.");
                }
            }
            return result;
        }

        struct TransformSet {
            DirectX::XMMATRIX point;
            DirectX::XMMATRIX normal;
            bool reverse_winding = false;
        };

        TransformSet make_transform_set(
            DirectX::FXMMATRIX transform) {

            DirectX::XMVECTOR determinant{};
            const DirectX::XMMATRIX inverse =
                DirectX::XMMatrixInverse(&determinant, transform);
            const float determinant_value =
                DirectX::XMVectorGetX(determinant);
            TransformSet result{
                transform,
                DirectX::XMMatrixTranspose(inverse),
                determinant_value < 0.0f,
            };
            return result;
        }

        DirectX::XMFLOAT3 transform_point(
            const DirectX::XMFLOAT3& value,
            DirectX::FXMMATRIX transform) {

            DirectX::XMFLOAT3 result{};
            DirectX::XMStoreFloat3(
                &result,
                DirectX::XMVector3TransformCoord(
                    DirectX::XMLoadFloat3(&value),
                    transform));
            return result;
        }

        DirectX::XMFLOAT3 transform_direction(
            const DirectX::XMFLOAT3& value,
            DirectX::FXMMATRIX transform) {

            DirectX::XMFLOAT3 result{};
            DirectX::XMStoreFloat3(
                &result,
                DirectX::XMVector3Normalize(
                    DirectX::XMVector3TransformNormal(
                        DirectX::XMLoadFloat3(&value),
                        transform)));
            return result;
        }

        DirectX::XMFLOAT3 face_normal(
            const DirectX::XMFLOAT3& a,
            const DirectX::XMFLOAT3& b,
            const DirectX::XMFLOAT3& c) {

            DirectX::XMFLOAT3 result{};
            DirectX::XMStoreFloat3(
                &result,
                DirectX::XMVector3Normalize(
                    DirectX::XMVector3Cross(
                        DirectX::XMVectorSubtract(
                            DirectX::XMLoadFloat3(&b),
                            DirectX::XMLoadFloat3(&a)),
                        DirectX::XMVectorSubtract(
                            DirectX::XMLoadFloat3(&c),
                            DirectX::XMLoadFloat3(&a)))));
            if (!std::isfinite(result.x) ||
                !std::isfinite(result.y) ||
                !std::isfinite(result.z)) {
                return { 0.0f, 1.0f, 0.0f };
            }
            return result;
        }

        struct VertexKey {
            uint32_t point = 0;
            uint32_t normal = INVALID_INDEX;
            uint32_t tangent = INVALID_INDEX;
            uint32_t uv = INVALID_INDEX;
            uint32_t flat_face = INVALID_INDEX;

            bool operator==(const VertexKey&) const = default;
        };

        struct VertexKeyHash {
            size_t operator()(const VertexKey& key) const noexcept {
                size_t hash = 14695981039346656037ull;
                const uint32_t values[] = {
                    key.point,
                    key.normal,
                    key.tangent,
                    key.uv,
                    key.flat_face,
                };
                for (const uint32_t value : values) {
                    hash ^= value;
                    hash *= 1099511628211ull;
                }
                return hash;
            }
        };

        struct PrimitiveBuildState {
            source::Primitive primitive;
            std::unordered_map<VertexKey, uint32_t, VertexKeyHash> vertices;
        };

        uint32_t material_id(
            SceneSourceData& semantic,
            const source::SourceReference& reference,
            const std::string& stable_id,
            const std::unordered_map<std::string, uint32_t>& material_ids,
            std::unordered_set<std::string>& diagnosed_material_ids) {

            if (stable_id.empty()) return 0;
            const auto found = material_ids.find(stable_id);
            if (found != material_ids.end()) {
                return found->second;
            }
            if (diagnosed_material_ids.insert(stable_id).second) {
                append_diagnostic(
                    semantic,
                    source::ConversionSeverity::Warning,
                    reference,
                    "unresolved_material_binding",
                    "Material binding is preserved by stable source ID, but "
                    "no material graph could be derived for the legacy "
                    "renderer; fallback material 0 is used.");
            }
            return 0;
        }

        std::vector<source::Primitive> triangulate_polygon_mesh(
            SceneSourceData& semantic,
            const source::PolygonMesh& mesh,
            const std::unordered_map<std::string, uint32_t>& material_ids,
            std::unordered_set<std::string>& diagnosed_material_ids,
            DirectX::FXMMATRIX transform) {

            const SupportedPrimvars primvars =
                classify_primvars(semantic, mesh);
            const TransformSet transforms =
                make_transform_set(transform);

            std::vector<uint32_t> face_materials(
                mesh.face_vertex_counts.size(),
                material_id(
                    semantic,
                    mesh.source,
                    mesh.bound_material_source_id,
                    material_ids,
                    diagnosed_material_ids));
            for (const source::MaterialSubset& subset :
                mesh.material_subsets) {
                const uint32_t subset_material = material_id(
                    semantic,
                    subset.source,
                    subset.material_source_id,
                    material_ids,
                    diagnosed_material_ids);
                for (const uint32_t face_id : subset.face_indices) {
                    if (face_id < face_materials.size()) {
                        face_materials[face_id] = subset_material;
                    }
                }
            }

            std::map<uint32_t, PrimitiveBuildState> states;
            uint64_t face_vertex_begin = 0;
            bool invalid_primvar_sample = false;
            bool degenerate_face = false;
            for (uint32_t face_id = 0;
                face_id < mesh.face_vertex_counts.size();
                ++face_id) {
                const uint32_t count =
                    mesh.face_vertex_counts[face_id];
                if (count < 3) {
                    degenerate_face = true;
                    face_vertex_begin += count;
                    continue;
                }

                const auto corner_point = [&](uint32_t corner) {
                    return transform_point(
                        mesh.points[
                            mesh.face_vertex_indices[
                                static_cast<size_t>(
                                    face_vertex_begin + corner)]],
                        transforms.point);
                };
                const DirectX::XMFLOAT3 generated_normal =
                    face_normal(
                        corner_point(0),
                        corner_point(1),
                        corner_point(2));

                PrimitiveBuildState& state =
                    states[face_materials[face_id]];
                state.primitive.material_id =
                    face_materials[face_id];

                const auto append_corner =
                    [&](uint32_t corner) -> uint32_t {
                    const uint32_t face_vertex_id =
                        static_cast<uint32_t>(
                            face_vertex_begin + corner);
                    const uint32_t point_id =
                        mesh.face_vertex_indices[face_vertex_id];

                    const std::optional<uint32_t> normal_id =
                        primvars.normal == nullptr
                        ? std::nullopt
                        : primvar_value_index(
                            *primvars.normal,
                            face_id,
                            face_vertex_id,
                            point_id);
                    const std::optional<uint32_t> tangent_id =
                        primvars.tangent == nullptr
                        ? std::nullopt
                        : primvar_value_index(
                            *primvars.tangent,
                            face_id,
                            face_vertex_id,
                            point_id);
                    const std::optional<uint32_t> uv_id =
                        primvars.uv0 == nullptr
                        ? std::nullopt
                        : primvar_value_index(
                            *primvars.uv0,
                            face_id,
                            face_vertex_id,
                            point_id);
                    if ((primvars.normal != nullptr && !normal_id) ||
                        (primvars.tangent != nullptr && !tangent_id) ||
                        (primvars.uv0 != nullptr && !uv_id)) {
                        invalid_primvar_sample = true;
                    }

                    VertexKey key{};
                    key.point = point_id;
                    key.normal = normal_id.value_or(INVALID_INDEX);
                    key.tangent = tangent_id.value_or(INVALID_INDEX);
                    key.uv = uv_id.value_or(INVALID_INDEX);
                    key.flat_face = primvars.normal == nullptr ||
                        !normal_id
                        ? face_id
                        : INVALID_INDEX;
                    const auto found = state.vertices.find(key);
                    if (found != state.vertices.end()) {
                        return found->second;
                    }

                    const uint32_t vertex_id =
                        static_cast<uint32_t>(
                            state.primitive.positions.size());
                    state.primitive.positions.push_back(
                        transform_point(
                            mesh.points[point_id],
                            transforms.point));

                    DirectX::XMFLOAT3 normal = generated_normal;
                    if (primvars.normal != nullptr && normal_id) {
                        DirectX::XMFLOAT3 sampled{};
                        if (primvar_component(
                                *primvars.normal,
                                *normal_id,
                                0,
                                sampled.x) &&
                            primvar_component(
                                *primvars.normal,
                                *normal_id,
                                1,
                                sampled.y) &&
                            primvar_component(
                                *primvars.normal,
                                *normal_id,
                                2,
                                sampled.z)) {
                            normal = transform_direction(
                                sampled,
                                transforms.normal);
                        }
                    }
                    state.primitive.normals.push_back(normal);

                    if (primvars.tangent != nullptr) {
                        DirectX::XMFLOAT4 tangent{
                            1.0f, 0.0f, 0.0f, 1.0f
                        };
                        if (tangent_id) {
                            DirectX::XMFLOAT3 direction{};
                            if (primvar_component(
                                    *primvars.tangent,
                                    *tangent_id,
                                    0,
                                    direction.x) &&
                                primvar_component(
                                    *primvars.tangent,
                                    *tangent_id,
                                    1,
                                    direction.y) &&
                                primvar_component(
                                    *primvars.tangent,
                                    *tangent_id,
                                    2,
                                    direction.z)) {
                                direction = transform_direction(
                                    direction,
                                    transforms.normal);
                                tangent.x = direction.x;
                                tangent.y = direction.y;
                                tangent.z = direction.z;
                                if (primvars.tangent->
                                        numeric_component_count >= 4) {
                                    primvar_component(
                                        *primvars.tangent,
                                        *tangent_id,
                                        3,
                                        tangent.w);
                                }
                            }
                        }
                        state.primitive.tangents.push_back(tangent);
                    }

                    if (primvars.uv0 != nullptr) {
                        DirectX::XMFLOAT2 uv{};
                        if (uv_id) {
                            primvar_component(
                                *primvars.uv0,
                                *uv_id,
                                0,
                                uv.x);
                            primvar_component(
                                *primvars.uv0,
                                *uv_id,
                                1,
                                uv.y);
                        }
                        state.primitive.uv0.push_back(uv);
                    }
                    state.vertices.emplace(key, vertex_id);
                    return vertex_id;
                };

                for (uint32_t triangle = 1;
                    triangle + 1 < count;
                    ++triangle) {
                    std::array<uint32_t, 3> corners = {
                        0,
                        triangle,
                        triangle + 1,
                    };
                    if (transforms.reverse_winding) {
                        std::swap(corners[1], corners[2]);
                    }
                    state.primitive.indices.push_back(
                        append_corner(corners[0]));
                    state.primitive.indices.push_back(
                        append_corner(corners[1]));
                    state.primitive.indices.push_back(
                        append_corner(corners[2]));
                }
                face_vertex_begin += count;
            }

            if (invalid_primvar_sample) {
                append_diagnostic(
                    semantic,
                    source::ConversionSeverity::Warning,
                    mesh.source,
                    "invalid_primvar_sample",
                    "At least one indexed/interpolated primvar sample was "
                    "outside its supported domain; legacy defaults were used.");
            }
            if (degenerate_face) {
                append_diagnostic(
                    semantic,
                    source::ConversionSeverity::Warning,
                    mesh.source,
                    "degenerate_polygon_skipped",
                    "A polygon with fewer than three vertices was retained "
                    "semantically but omitted from triangle materialization.");
            }
            if (mesh.subdivision_scheme != "none" &&
                !mesh.subdivision_scheme.empty()) {
                append_diagnostic(
                    semantic,
                    source::ConversionSeverity::Info,
                    mesh.source,
                    "subdivision_not_evaluated",
                    "Authored subdivision topology remains semantic; legacy "
                    "materialization triangulates the control cage.");
            }

            std::vector<source::Primitive> primitives;
            primitives.reserve(states.size());
            for (auto& [unused_material, state] : states) {
                (void)unused_material;
                if (!state.primitive.indices.empty()) {
                    primitives.push_back(std::move(state.primitive));
                }
            }
            return primitives;
        }

        DirectX::XMFLOAT4X4 identity_matrix() {
            DirectX::XMFLOAT4X4 result{};
            DirectX::XMStoreFloat4x4(
                &result,
                DirectX::XMMatrixIdentity());
            return result;
        }

        DirectX::XMFLOAT4X4 local_from_world(
            const source::Node& node,
            const source::Node* parent) {

            if (!node.reset_xform_stack || parent == nullptr) {
                return node.local_transform;
            }
            const DirectX::XMMATRIX parent_inverse =
                DirectX::XMMatrixInverse(
                    nullptr,
                    DirectX::XMLoadFloat4x4(
                        &parent->world_transform));
            DirectX::XMFLOAT4X4 result{};
            DirectX::XMStoreFloat4x4(
                &result,
                DirectX::XMMatrixMultiply(
                    DirectX::XMLoadFloat4x4(
                        &node.world_transform),
                    parent_inverse));
            return result;
        }

        DirectX::XMFLOAT4 default_orientation() {
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        }

        DirectX::XMFLOAT3 default_scale() {
            return { 1.0f, 1.0f, 1.0f };
        }

        bool is_identity_matrix(
            const DirectX::XMFLOAT4X4& matrix,
            float tolerance = 1.0e-5f) {

            const DirectX::XMFLOAT4X4 identity =
                identity_matrix();
            const float* values = &matrix._11;
            const float* identity_values = &identity._11;
            for (size_t index = 0; index < 16; ++index) {
                if (std::abs(
                        values[index] -
                        identity_values[index]) >
                    tolerance) {
                    return false;
                }
            }
            return true;
        }

    } // namespace

    JungleSceneMaterialization JungleSceneCPUBuilder::materialize(
        SceneSourceData& semantic_scene,
        const JungleSceneMaterializationOptions& options) {

        JungleSceneMaterialization result{};
        SceneSourceData& legacy = result.legacy_scene;
        result.semantic_node_to_legacy_node.assign(
            semantic_scene.nodes.size(),
            INVALID_INDEX);

        const auto material_ids =
            derive_materials(semantic_scene, legacy);
        std::unordered_set<std::string> diagnosed_material_ids;

        uint64_t point_instance_total = 0;
        for (const source::PointInstancer& instancer :
            semantic_scene.point_instancers) {
            if (instancer.logical_instance_count >
                (std::numeric_limits<uint32_t>::max)() -
                    point_instance_total) {
                fail_materialization(
                    semantic_scene,
                    instancer.source,
                    "point_instance_32bit_overflow",
                    "PointInstancer expansion exceeds the legacy 32-bit "
                    "instance contract.");
            }
            point_instance_total += instancer.logical_instance_count;
        }
        if (options.expand_point_instancers &&
            semantic_scene.nodes.size() >
                (std::numeric_limits<uint32_t>::max)() -
                    point_instance_total) {
            fail_materialization(
                semantic_scene,
                {},
                "scene_instance_32bit_overflow",
                "Expanded point instances plus ordinary scene nodes exceed "
                "the legacy 32-bit instance contract.");
        }
        result.logical_point_instance_count = point_instance_total;
        result.expanded_point_instance_count =
            options.expand_point_instancers
            ? point_instance_total
            : 0;
        result.native_instance_count =
            semantic_scene.native_instances.size();
        if (options.expand_point_instancers) {
            legacy.instances.resize(
                static_cast<size_t>(point_instance_total));
        }

        std::vector<uint8_t> excluded(
            semantic_scene.nodes.size(),
            0);
        std::unordered_map<std::string, uint32_t>
            prototype_roots;
        for (const source::NativePrototype& prototype :
            semantic_scene.native_prototypes) {
            prototype_roots[prototype.source.stable_id] =
                prototype.root_node_id;
            for (const uint32_t node_id : prototype.node_ids) {
                if (node_id < excluded.size()) {
                    excluded[node_id] = 1;
                }
            }
        }

        std::unordered_map<std::string, uint32_t> nodes_by_source_id;
        for (uint32_t node_id = 0;
            node_id < semantic_scene.nodes.size();
            ++node_id) {
            nodes_by_source_id.emplace(
                semantic_scene.nodes[node_id].source.stable_id,
                node_id);
        }

        std::unordered_map<uint32_t, std::string>
            native_prototype_by_node;
        for (const source::NativeInstance& instance :
            semantic_scene.native_instances) {
            native_prototype_by_node.emplace(
                instance.node_id,
                instance.prototype_source_id);
            if (instance.has_composed_overrides) {
                append_diagnostic(
                    semantic_scene,
                    source::ConversionSeverity::Info,
                    instance.source,
                    "native_instance_composed_overrides",
                    "Native instance geometry shares its composed prototype "
                    "mesh; the semantic instance retains the authored "
                    "composition relationship.");
            }
        }

        std::vector<uint8_t> suppress_direct_geometry(
            semantic_scene.nodes.size(),
            0);
        const auto mark_subtree =
            [&](auto&& self, uint32_t node_id) -> void {
            if (node_id >= suppress_direct_geometry.size() ||
                suppress_direct_geometry[node_id]) {
                return;
            }
            suppress_direct_geometry[node_id] = 1;
            for (const uint32_t child :
                semantic_scene.nodes[node_id].children) {
                self(self, child);
            }
        };
        for (const source::PointInstancer& instancer :
            semantic_scene.point_instancers) {
            for (const std::string& prototype_id :
                instancer.prototype_source_ids) {
                const auto found = nodes_by_source_id.find(
                    prototype_id);
                if (found != nodes_by_source_id.end()) {
                    mark_subtree(mark_subtree, found->second);
                }
            }
        }

        std::unordered_map<uint32_t, uint32_t>
            standalone_mesh_ids;
        const auto standalone_mesh =
            [&](uint32_t polygon_mesh_id) -> uint32_t {
            const auto found =
                standalone_mesh_ids.find(polygon_mesh_id);
            if (found != standalone_mesh_ids.end()) {
                return found->second;
            }
            if (polygon_mesh_id >=
                semantic_scene.polygon_meshes.size()) {
                return INVALID_INDEX;
            }
            const source::PolygonMesh& polygon =
                semantic_scene.polygon_meshes[polygon_mesh_id];
            source::Mesh mesh{};
            mesh.name = polygon.source.prim_path;
            mesh.primitives = triangulate_polygon_mesh(
                semantic_scene,
                polygon,
                material_ids,
                diagnosed_material_ids,
                DirectX::XMMatrixIdentity());
            if (mesh.primitives.empty()) {
                append_diagnostic(
                    semantic_scene,
                    source::ConversionSeverity::Error,
                    polygon.source,
                    "empty_triangle_materialization",
                    "Polygon mesh produced no legacy triangles.");
                return INVALID_INDEX;
            }
            const uint32_t mesh_id =
                static_cast<uint32_t>(legacy.meshes.size());
            legacy.meshes.push_back(std::move(mesh));
            standalone_mesh_ids.emplace(
                polygon_mesh_id,
                mesh_id);
            return mesh_id;
        };

        const auto resolve_prototype_root =
            [&](const std::string& source_id) -> uint32_t {
            const auto native =
                prototype_roots.find(source_id);
            if (native != prototype_roots.end()) {
                return native->second;
            }
            const auto ordinary =
                nodes_by_source_id.find(source_id);
            if (ordinary != nodes_by_source_id.end()) {
                return ordinary->second;
            }
            return INVALID_INDEX;
        };

        std::unordered_map<std::string, uint32_t>
            shared_prototype_mesh_ids;
        std::unordered_set<std::string>
            active_shared_prototype_ids;
        std::function<uint32_t(const std::string&)>
            shared_prototype_mesh;
        shared_prototype_mesh =
            [&](const std::string& prototype_id) -> uint32_t {
            const auto cached =
                shared_prototype_mesh_ids.find(prototype_id);
            if (cached != shared_prototype_mesh_ids.end()) {
                return cached->second;
            }

            if (!active_shared_prototype_ids.insert(
                    prototype_id).second) {
                fail_materialization(
                    semantic_scene,
                    {},
                    "prototype_alias_cycle",
                    "Jungle prototype materialization encountered a cyclic "
                    "prototype alias.");
            }

            const uint32_t root_node_id =
                resolve_prototype_root(prototype_id);
            if (root_node_id >= semantic_scene.nodes.size()) {
                fail_materialization(
                    semantic_scene,
                    {},
                    "unresolved_prototype",
                    "A NativeInstance/PointInstancer prototype source ID "
                    "does not resolve to semantic geometry.");
            }

            const auto direct_native_instance =
                native_prototype_by_node.find(root_node_id);
            if (direct_native_instance !=
                native_prototype_by_node.end()) {
                const uint32_t mesh_id =
                    shared_prototype_mesh(
                        direct_native_instance->second);
                shared_prototype_mesh_ids.emplace(
                    prototype_id,
                    mesh_id);
                active_shared_prototype_ids.erase(prototype_id);
                append_diagnostic(
                    semantic_scene,
                    source::ConversionSeverity::Info,
                    semantic_scene.nodes[root_node_id].source,
                    "prototype_mesh_alias",
                    "A Jungle prototype rooted at a native instance reuses "
                    "the referenced native prototype mesh instead of "
                    "duplicating its geometry.");
                return mesh_id;
            }

            source::Mesh mesh{};
            mesh.name = "Jungle shared prototype " + prototype_id;

            std::unordered_set<uint32_t> active_prototype_roots;
            std::function<void(
                uint32_t,
                const DirectX::XMFLOAT4X4&)>
                append_prototype_geometry;
            append_prototype_geometry =
                [&](uint32_t current_root_id,
                    const DirectX::XMFLOAT4X4&
                        current_root_to_output) {

                if (!active_prototype_roots.insert(
                        current_root_id).second) {
                    fail_materialization(
                        semantic_scene,
                        semantic_scene.nodes[current_root_id].source,
                        "prototype_reference_cycle",
                        "Jungle prototype materialization encountered a "
                        "cyclic native-instance prototype reference.");
                }

                const DirectX::XMMATRIX current_root_inverse =
                    DirectX::XMMatrixInverse(
                        nullptr,
                        DirectX::XMLoadFloat4x4(
                            &semantic_scene.nodes[current_root_id].
                                world_transform));
                const DirectX::XMMATRIX root_to_output =
                    DirectX::XMLoadFloat4x4(
                        &current_root_to_output);
                const auto append_node_geometry =
                    [&](auto&& self, uint32_t node_id) -> void {
                    const source::Node& node =
                        semantic_scene.nodes[node_id];
                    const DirectX::XMMATRIX node_to_current_root =
                        DirectX::XMMatrixMultiply(
                            DirectX::XMLoadFloat4x4(
                                &node.world_transform),
                            current_root_inverse);
                    const DirectX::XMMATRIX node_to_output =
                        DirectX::XMMatrixMultiply(
                            node_to_current_root,
                            root_to_output);

                    if (node.polygon_mesh_id != INVALID_INDEX) {
                        std::vector<source::Primitive> primitives =
                            triangulate_polygon_mesh(
                                semantic_scene,
                                semantic_scene.polygon_meshes[
                                    node.polygon_mesh_id],
                                material_ids,
                                diagnosed_material_ids,
                                node_to_output);
                        mesh.primitives.insert(
                            mesh.primitives.end(),
                            std::make_move_iterator(
                                primitives.begin()),
                            std::make_move_iterator(
                                primitives.end()));
                    }

                    const auto nested_instance =
                        native_prototype_by_node.find(node_id);
                    if (nested_instance !=
                        native_prototype_by_node.end()) {
                        const uint32_t nested_root_id =
                            resolve_prototype_root(
                                nested_instance->second);
                        if (nested_root_id >=
                            semantic_scene.nodes.size()) {
                            fail_materialization(
                                semantic_scene,
                                node.source,
                                "unresolved_nested_prototype",
                                "Nested Jungle native-instance prototype '" +
                                nested_instance->second +
                                "' does not resolve to semantic geometry.");
                        }
                        DirectX::XMFLOAT4X4
                            nested_root_to_output{};
                        DirectX::XMStoreFloat4x4(
                            &nested_root_to_output,
                            node_to_output);
                        append_prototype_geometry(
                            nested_root_id,
                            nested_root_to_output);
                        return;
                    }

                    for (const uint32_t child : node.children) {
                        self(self, child);
                    }
                };
                append_node_geometry(
                    append_node_geometry,
                    current_root_id);
                active_prototype_roots.erase(current_root_id);
            };

            DirectX::XMFLOAT4X4 identity{};
            DirectX::XMStoreFloat4x4(
                &identity,
                DirectX::XMMatrixIdentity());
            append_prototype_geometry(
                root_node_id,
                identity);
            compact_primitives_by_material(
                semantic_scene,
                mesh,
                semantic_scene.nodes[root_node_id].source);
            if (mesh.primitives.empty()) {
                fail_materialization(
                    semantic_scene,
                    semantic_scene.nodes[root_node_id].source,
                    "prototype_has_no_geometry",
                    "Referenced Jungle prototype '" + prototype_id +
                    "' rooted at '" +
                    semantic_scene.nodes[root_node_id].source.prim_path +
                    "' produced no legacy triangle geometry; it cannot be "
                    "silently skipped.");
            }

            const uint32_t mesh_id =
                static_cast<uint32_t>(legacy.meshes.size());
            legacy.meshes.push_back(std::move(mesh));
            shared_prototype_mesh_ids.emplace(
                prototype_id,
                mesh_id);
            active_shared_prototype_ids.erase(prototype_id);
            return mesh_id;
        };

        std::unordered_map<uint32_t, std::vector<uint32_t>>
            point_instancers_by_node;
        for (uint32_t instancer_id = 0;
            instancer_id < semantic_scene.point_instancers.size();
            ++instancer_id) {
            point_instancers_by_node[
                semantic_scene.point_instancers[instancer_id].node_id].
                    push_back(instancer_id);
        }

        uint64_t instance_write_base = 0;
        std::function<uint32_t(uint32_t, uint32_t)> clone_node;
        clone_node =
            [&](uint32_t semantic_node_id,
                uint32_t legacy_parent_id) -> uint32_t {
            if (semantic_node_id >= semantic_scene.nodes.size() ||
                excluded[semantic_node_id]) {
                return INVALID_INDEX;
            }

            const source::Node& input =
                semantic_scene.nodes[semantic_node_id];
            source::Node output{};
            output.name = input.name;
            output.source = input.source;
            output.stable_id = input.stable_id;
            output.local_transform = local_from_world(
                input,
                input.parent_node_id < semantic_scene.nodes.size()
                    ? &semantic_scene.nodes[input.parent_node_id]
                    : nullptr);
            output.parent_node_id = legacy_parent_id;

            const auto native =
                native_prototype_by_node.find(semantic_node_id);
            if (!suppress_direct_geometry[semantic_node_id]) {
                if (native != native_prototype_by_node.end()) {
                    output.mesh_id =
                        shared_prototype_mesh(native->second);
                }
                else if (input.polygon_mesh_id != INVALID_INDEX) {
                    output.mesh_id =
                        standalone_mesh(input.polygon_mesh_id);
                }
            }

            const uint32_t legacy_node_id =
                static_cast<uint32_t>(legacy.nodes.size());
            legacy.nodes.push_back(std::move(output));
            result.semantic_node_to_legacy_node[semantic_node_id] =
                legacy_node_id;
            if (legacy_parent_id == INVALID_INDEX) {
                legacy.root_node_id = legacy_node_id;
            }
            else {
                legacy.nodes[legacy_parent_id].children.push_back(
                    legacy_node_id);
            }

            for (const uint32_t child_id : input.children) {
                clone_node(child_id, legacy_node_id);
            }

            const auto point_range =
                point_instancers_by_node.find(semantic_node_id);
            if (point_range == point_instancers_by_node.end()) {
                return legacy_node_id;
            }
            for (const uint32_t point_instancer_id :
                point_range->second) {
                const source::PointInstancer& instancer =
                    semantic_scene.point_instancers[
                        point_instancer_id];
                if (!instancer.inactive_ids.empty() ||
                    !instancer.invisible_ids.empty()) {
                    append_diagnostic(
                        semantic_scene,
                        source::ConversionSeverity::Info,
                        instancer.source,
                        options.expand_point_instancers
                            ? "point_instance_visibility_legacy_expansion"
                            : "point_instance_visibility_compact_stream",
                        options.expand_point_instancers
                            ? "Inactive/invisible IDs remain in semantic "
                                "data; all logical PointInstancer entries "
                                "are explicitly expanded because the legacy "
                                "instance contract has no per-instance "
                                "visibility field."
                            : "Inactive/invisible IDs remain in semantic "
                                "data and the compact PointInstancer stream "
                                "keeps every logical entry without an "
                                "implicit skip or cap.");
                }
                if (instancer.time_varying) {
                    append_diagnostic(
                        semantic_scene,
                        source::ConversionSeverity::Info,
                        instancer.source,
                        "point_instance_time_sample",
                        "The legacy instance stream materializes only the "
                        "semantic scene's evaluated time code.");
                }

                std::vector<uint64_t> offsets(
                    instancer.prototype_source_ids.size(),
                    0);
                std::vector<uint64_t> cursors(
                    instancer.prototype_source_ids.size(),
                    0);
                std::vector<DirectX::XMFLOAT4X4>
                    prototype_local_transforms(
                        instancer.prototype_source_ids.size(),
                        identity_matrix());
                std::vector<uint8_t> prototype_has_transform(
                    instancer.prototype_source_ids.size(),
                    0);
                uint64_t local_offset = 0;
                for (uint32_t prototype_index = 0;
                    prototype_index <
                        instancer.prototype_source_ids.size();
                    ++prototype_index) {
                    offsets[prototype_index] =
                        instance_write_base + local_offset;
                    local_offset +=
                        instancer.prototype_instance_counts[
                            prototype_index];

                    source::Node prototype_node{};
                    prototype_node.name =
                        "PointInstancer prototype " +
                        std::to_string(prototype_index);
                    prototype_node.local_transform =
                        identity_matrix();
                    prototype_node.parent_node_id =
                        legacy_node_id;
                    prototype_node.mesh_id =
                        shared_prototype_mesh(
                            instancer.prototype_source_ids[
                                prototype_index]);
                    const uint32_t prototype_root_id =
                        resolve_prototype_root(
                            instancer.prototype_source_ids[
                                prototype_index]);
                    if (prototype_root_id >=
                        semantic_scene.nodes.size()) {
                        fail_materialization(
                            semantic_scene,
                            instancer.source,
                            "unresolved_point_prototype",
                            "A Jungle PointInstancer prototype source ID "
                            "does not resolve to a semantic node.");
                    }
                    prototype_local_transforms[prototype_index] =
                        semantic_scene.nodes[
                            prototype_root_id].local_transform;
                    prototype_has_transform[prototype_index] =
                        !is_identity_matrix(
                            prototype_local_transforms[
                                prototype_index]);
                    if (prototype_has_transform[
                            prototype_index]) {
                        append_diagnostic(
                            semantic_scene,
                            source::ConversionSeverity::Info,
                            semantic_scene.nodes[
                                prototype_root_id].source,
                            options.expand_point_instancers
                                ? "point_prototype_matrix_materialized"
                                : "point_prototype_affine_preserved",
                            options.expand_point_instancers
                                ? "The PointInstancer prototype root "
                                    "transform is composed into an explicit "
                                    "affine legacy instance matrix so "
                                    "non-TRS transforms remain exact."
                                : "The PointInstancer prototype root affine "
                                    "transform remains separate from compact "
                                    "point TRS data so shear stays exact "
                                    "without geometry or instance expansion.");
                    }
                    result.point_prototypes.push_back({
                        point_instancer_id,
                        prototype_index,
                        prototype_node.mesh_id,
                        prototype_local_transforms[prototype_index]
                    });
                    if (!options.expand_point_instancers) {
                        continue;
                    }
                    prototype_node.first_instance =
                        static_cast<uint32_t>(
                            offsets[prototype_index]);
                    prototype_node.instance_count =
                        static_cast<uint32_t>(
                            instancer.prototype_instance_counts[
                                prototype_index]);

                    const uint32_t prototype_node_id =
                        static_cast<uint32_t>(
                            legacy.nodes.size());
                    legacy.nodes.push_back(
                        std::move(prototype_node));
                    legacy.nodes[legacy_node_id].children.push_back(
                        prototype_node_id);
                }

                if (options.expand_point_instancers) {
                    for (uint32_t source_index = 0;
                        source_index <
                            instancer.logical_instance_count;
                        ++source_index) {
                        const uint32_t prototype_index =
                            static_cast<uint32_t>(
                                instancer.proto_indices[
                                    source_index]);
                        const uint64_t destination_index =
                            offsets[prototype_index] +
                            cursors[prototype_index]++;
                        source::InstanceTransform& destination =
                            legacy.instances[
                                static_cast<size_t>(
                                    destination_index)];
                        const DirectX::XMFLOAT3 translation =
                            instancer.positions[source_index];
                        const DirectX::XMFLOAT4 rotation =
                            instancer.orientations.empty()
                            ? default_orientation()
                            : instancer.orientations[
                                source_index];
                        const DirectX::XMFLOAT3 scale =
                            instancer.scales.empty()
                            ? default_scale()
                            : instancer.scales[source_index];
                        destination.translation = translation;
                        destination.rotation = rotation;
                        destination.scale = scale;
                        destination.source_index = source_index;
                        if (prototype_has_transform[
                                prototype_index]) {
                            const DirectX::XMMATRIX point_transform =
                                DirectX::XMMatrixMultiply(
                                    DirectX::XMMatrixScaling(
                                        scale.x,
                                        scale.y,
                                        scale.z),
                                    DirectX::XMMatrixMultiply(
                                        DirectX::XMMatrixRotationQuaternion(
                                            DirectX::XMLoadFloat4(
                                                &rotation)),
                                        DirectX::XMMatrixTranslation(
                                            translation.x,
                                            translation.y,
                                            translation.z)));
                            DirectX::XMStoreFloat4x4(
                                &destination.matrix,
                                DirectX::XMMatrixMultiply(
                                    DirectX::XMLoadFloat4x4(
                                        &prototype_local_transforms[
                                            prototype_index]),
                                    point_transform));
                            destination.has_matrix = true;
                        }
                    }
                    instance_write_base +=
                        instancer.logical_instance_count;
                }
            }
            return legacy_node_id;
        };

        clone_node(
            semantic_scene.root_node_id,
            INVALID_INDEX);
        if (options.expand_point_instancers &&
            instance_write_base != point_instance_total) {
            fail_materialization(
                semantic_scene,
                {},
                "point_instance_expansion_mismatch",
                "PointInstancer expansion did not consume every logical "
                "instance.");
        }
        if (legacy.meshes.empty()) {
            fail_materialization(
                semantic_scene,
                {},
                "no_legacy_geometry",
                "Jungle semantic scene produced no legacy triangle meshes.");
        }

        uint64_t materialized_instance_count = 0;
        uint64_t materialized_draw_instance_count = 0;
        for (const source::Node& node : legacy.nodes) {
            if (node.mesh_id == INVALID_INDEX) {
                continue;
            }
            if (node.mesh_id >= legacy.meshes.size()) {
                fail_materialization(
                    semantic_scene,
                    node.source,
                    "invalid_legacy_mesh_reference",
                    "Jungle materialization produced an invalid legacy "
                    "mesh reference.");
            }

            const uint64_t instance_count =
                node.instance_count == 0
                ? 1
                : node.instance_count;
            const uint64_t submesh_count =
                legacy.meshes[node.mesh_id].primitives.size();
            if (instance_count >
                (std::numeric_limits<uint64_t>::max)() /
                    (std::max)(uint64_t{ 1 }, submesh_count) ||
                materialized_draw_instance_count >
                (std::numeric_limits<uint64_t>::max)() -
                    instance_count * submesh_count) {
                fail_materialization(
                    semantic_scene,
                    node.source,
                    "draw_instance_64bit_overflow",
                    "Jungle draw-instance preflight exceeds 64-bit "
                    "accounting.");
            }
            materialized_instance_count += instance_count;
            materialized_draw_instance_count +=
                instance_count * submesh_count;
        }
        if (materialized_instance_count >
            (std::numeric_limits<uint32_t>::max)()) {
            fail_materialization(
                semantic_scene,
                {},
                "materialized_instance_32bit_overflow",
                "Jungle materialization requires " +
                std::to_string(materialized_instance_count) +
                " instances, exceeding the legacy 32-bit instance "
                "contract.");
        }
        if (materialized_draw_instance_count >
            (std::numeric_limits<uint32_t>::max)()) {
            fail_materialization(
                semantic_scene,
                {},
                "draw_instance_32bit_overflow",
                "Jungle materialization requires " +
                std::to_string(materialized_draw_instance_count) +
                " instance/submesh draw records, exceeding the legacy "
                "32-bit CPU/GPU contract.");
        }

        result.materialized_instance_count =
            materialized_instance_count;
        result.materialized_draw_instance_count =
            materialized_draw_instance_count;
        std::unordered_set<uint32_t> unique_shared_mesh_ids;
        for (const auto& [prototype_id, mesh_id] :
            shared_prototype_mesh_ids) {
            (void)prototype_id;
            unique_shared_mesh_ids.insert(mesh_id);
        }
        result.shared_prototype_mesh_count =
            static_cast<uint32_t>(
                unique_shared_mesh_ids.size());
        legacy.conversion_diagnostics =
            semantic_scene.conversion_diagnostics;
        util::Logger::g_logger <<
            "Jungle CPU materialization: meshes=" <<
            legacy.meshes.size() <<
            ", native_instances=" <<
            result.native_instance_count <<
            ", logical_point_instances=" <<
            result.logical_point_instance_count <<
            ", expanded_point_instances=" <<
            result.expanded_point_instance_count <<
            ", materialized_instances=" <<
            result.materialized_instance_count <<
            ", draw_instances=" <<
            result.materialized_draw_instance_count <<
            ", shared_prototype_meshes=" <<
            result.shared_prototype_mesh_count << '\n';
        return result;
    }

    SceneCPUData JungleSceneCPUBuilder::build(
        SceneSourceData& semantic_scene) {

        JungleSceneMaterialization materialized =
            materialize(semantic_scene);
        return SceneCPUBuilder::build(
            materialized.legacy_scene);
    }

    JungleSceneCPUData JungleSceneCPUBuilder::build_compact(
        SceneSourceData& semantic_scene) {

        JungleSceneMaterializationOptions options{};
        options.expand_point_instancers = false;
        JungleSceneMaterialization materialized =
            materialize(semantic_scene, options);

        JungleSceneCPUData result{};
        result.scene = SceneCPUBuilder::build(
            materialized.legacy_scene);
        result.logical_point_instance_count =
            materialized.logical_point_instance_count;
        result.point_instances.reserve(
            static_cast<size_t>(
                materialized.logical_point_instance_count));
        result.point_instance_ids_by_prototype.resize(
            static_cast<size_t>(
                materialized.logical_point_instance_count));
        result.point_prototypes.reserve(
            materialized.point_prototypes.size());

        const auto prototype_key =
            [](uint32_t point_instancer_id,
                uint32_t prototype_index) -> uint64_t {
            return (
                static_cast<uint64_t>(point_instancer_id) << 32) |
                prototype_index;
        };
        std::unordered_map<
            uint64_t,
            const JunglePointPrototypeMaterialization*>
            materialized_prototypes;
        for (const auto& prototype :
            materialized.point_prototypes) {
            materialized_prototypes.emplace(
                prototype_key(
                    prototype.point_instancer_id,
                    prototype.prototype_index),
                &prototype);
        }

        uint64_t point_instance_base = 0;
        uint64_t point_id_base = 0;
        for (uint32_t point_instancer_id = 0;
            point_instancer_id <
                semantic_scene.point_instancers.size();
            ++point_instancer_id) {
            const source::PointInstancer& instancer =
                semantic_scene.point_instancers[
                    point_instancer_id];
            if (instancer.node_id >= semantic_scene.nodes.size()) {
                fail_materialization(
                    semantic_scene,
                    instancer.source,
                    "compact_instancer_node_invalid",
                    "A compact Jungle PointInstancer references an invalid "
                    "semantic node.");
            }

            for (uint32_t source_index = 0;
                source_index < instancer.logical_instance_count;
                ++source_index) {
                JungleSceneCPUData::PointInstance instance{};
                instance.translation =
                    instancer.positions[source_index];
                instance.source_index = source_index;
                instance.rotation =
                    instancer.orientations.empty()
                    ? default_orientation()
                    : instancer.orientations[source_index];
                instance.scale =
                    instancer.scales.empty()
                    ? default_scale()
                    : instancer.scales[source_index];
                result.point_instances.push_back(instance);
            }

            std::vector<uint64_t> offsets(
                instancer.prototype_source_ids.size(),
                0);
            std::vector<uint64_t> cursors(
                instancer.prototype_source_ids.size(),
                0);
            uint64_t local_offset = 0;
            for (uint32_t prototype_index = 0;
                prototype_index <
                    instancer.prototype_source_ids.size();
                ++prototype_index) {
                const auto found =
                    materialized_prototypes.find(
                        prototype_key(
                            point_instancer_id,
                            prototype_index));
                if (found == materialized_prototypes.end()) {
                    fail_materialization(
                        semantic_scene,
                        instancer.source,
                        "compact_prototype_missing",
                        "A compact Jungle PointInstancer prototype has no "
                        "materialized shared mesh.");
                }

                offsets[prototype_index] =
                    point_id_base + local_offset;
                const uint64_t prototype_count =
                    instancer.prototype_instance_counts[
                        prototype_index];
                if (offsets[prototype_index] >
                        (std::numeric_limits<uint32_t>::max)() ||
                    prototype_count >
                        (std::numeric_limits<uint32_t>::max)()) {
                    fail_materialization(
                        semantic_scene,
                        instancer.source,
                        "compact_prototype_range_overflow",
                        "A compact Jungle prototype instance range exceeds "
                        "32-bit GPU indexing.");
                }

                JungleSceneCPUData::PointPrototype prototype{};
                prototype.mesh_id = found->second->mesh_id;
                prototype.first_instance_id =
                    static_cast<uint32_t>(
                        offsets[prototype_index]);
                prototype.instance_count =
                    static_cast<uint32_t>(
                        prototype_count);
                prototype.point_instancer_id =
                    point_instancer_id;
                prototype.prototype_local_transform =
                    found->second->prototype_local_transform;
                prototype.instancer_world_transform =
                    semantic_scene.nodes[
                        instancer.node_id].world_transform;
                result.point_prototypes.push_back(prototype);
                local_offset += prototype_count;
            }

            for (uint32_t source_index = 0;
                source_index < instancer.logical_instance_count;
                ++source_index) {
                const int32_t authored_prototype_index =
                    instancer.proto_indices[source_index];
                if (authored_prototype_index < 0 ||
                    static_cast<size_t>(
                        authored_prototype_index) >=
                        instancer.prototype_source_ids.size()) {
                    fail_materialization(
                        semantic_scene,
                        instancer.source,
                        "compact_proto_index_invalid",
                        "A compact Jungle PointInstancer entry references "
                        "an invalid prototype index.");
                }
                const uint32_t prototype_index =
                    static_cast<uint32_t>(
                        authored_prototype_index);
                const uint64_t destination_index =
                    offsets[prototype_index] +
                    cursors[prototype_index]++;
                const uint64_t point_instance_id =
                    point_instance_base + source_index;
                if (destination_index >=
                        result.point_instance_ids_by_prototype.size() ||
                    point_instance_id >
                        (std::numeric_limits<uint32_t>::max)()) {
                    fail_materialization(
                        semantic_scene,
                        instancer.source,
                        "compact_point_id_overflow",
                        "A compact Jungle PointInstancer ID exceeds the "
                        "allocated 32-bit stream.");
                }
                result.point_instance_ids_by_prototype[
                    static_cast<size_t>(destination_index)] =
                        static_cast<uint32_t>(
                            point_instance_id);
            }

            for (uint32_t prototype_index = 0;
                prototype_index < cursors.size();
                ++prototype_index) {
                if (cursors[prototype_index] !=
                    instancer.prototype_instance_counts[
                        prototype_index]) {
                    fail_materialization(
                        semantic_scene,
                        instancer.source,
                        "compact_prototype_count_mismatch",
                        "Compact Jungle PointInstancer grouping did not "
                        "consume the authored prototype count.");
                }
            }
            point_instance_base +=
                instancer.logical_instance_count;
            point_id_base +=
                instancer.logical_instance_count;
        }

        if (point_instance_base !=
                materialized.logical_point_instance_count ||
            point_id_base !=
                materialized.logical_point_instance_count ||
            result.point_instances.size() !=
                materialized.logical_point_instance_count) {
            fail_materialization(
                semantic_scene,
                {},
                "compact_point_instance_count_mismatch",
                "Compact Jungle PointInstancer materialization did not "
                "preserve every logical instance.");
        }

        const uint64_t compact_bytes =
            static_cast<uint64_t>(
                result.point_instances.size()) *
                sizeof(JungleSceneCPUData::PointInstance) +
            static_cast<uint64_t>(
                result.point_instance_ids_by_prototype.size()) *
                sizeof(uint32_t) +
            static_cast<uint64_t>(
                result.point_prototypes.size()) *
                sizeof(JungleSceneCPUData::PointPrototype);
        util::Logger::g_logger <<
            "Jungle compact CPU stream: ordinary_instances=" <<
            result.scene.instances.size() <<
            ", logical_point_instances=" <<
            result.logical_point_instance_count <<
            ", point_prototypes=" <<
            result.point_prototypes.size() <<
            ", compact_bytes=" << compact_bytes << '\n';
        return result;
    }

} // namespace scene
