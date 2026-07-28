#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "math/AABB.h"

namespace scene::source {

    struct SourceReference {
        std::string stable_id;
        std::string prim_path;
        std::string property_path;
        std::string layer_identifier;
        std::string source_type;
    };

    enum class ConversionSeverity : uint8_t {
        Info,
        Warning,
        Error,
        Fatal
    };

    struct ConversionDiagnostic {
        ConversionSeverity severity = ConversionSeverity::Info;
        SourceReference source;
        std::string code;
        std::string message;
    };

    struct SceneMetadata {
        std::string source_scene_identifier;
        std::string root_usd_path;
        std::string default_prim_path;
        std::string up_axis;
        std::string coordinate_system;
        double meters_per_unit = 1.0;
        double start_time_code = 0.0;
        double end_time_code = 0.0;
        double frames_per_second = 24.0;
        double time_codes_per_second = 24.0;
        double evaluated_time_code = 0.0;
        math::AABB source_bounds{};
    };

    enum class PrimvarInterpolation : uint8_t {
        Constant,
        Uniform,
        Vertex,
        Varying,
        FaceVarying,
        Instance,
        Unknown
    };

    // Generic numeric values are kept in their declared scalar domain. Other
    // authored values remain traceable through serialized_value and source.
    struct Primvar {
        SourceReference source;
        std::string name;
        std::string value_type;
        PrimvarInterpolation interpolation = PrimvarInterpolation::Unknown;
        uint32_t element_size = 1;
        uint32_t numeric_component_count = 0;
        std::vector<float> float_values;
        std::vector<double> double_values;
        std::vector<int64_t> integer_values;
        std::vector<uint32_t> indices;
        std::string serialized_value;
        bool indexed = false;
        bool time_varying = false;
    };

    struct MaterialSubset {
        SourceReference source;
        std::vector<uint32_t> face_indices;
        std::string material_source_id;
    };

    // Polygon topology is deliberately separate from the legacy triangle mesh
    // representation consumed by SceneCPUBuilder.
    struct PolygonMesh {
        SourceReference source;
        std::vector<DirectX::XMFLOAT3> points;
        std::vector<uint32_t> face_vertex_counts;
        std::vector<uint32_t> face_vertex_indices;
        std::string subdivision_scheme;
        std::string orientation;
        bool double_sided = false;
        math::AABB local_bounds{};
        std::vector<Primvar> primvars;
        std::vector<MaterialSubset> material_subsets;
        std::string bound_material_source_id;
        double evaluated_time_code = 0.0;
        bool time_varying = false;
    };

    struct NativePrototype {
        SourceReference source;
        uint32_t root_node_id = 0;
        std::vector<uint32_t> node_ids;
    };

    struct NativeInstance {
        SourceReference source;
        uint32_t node_id = 0;
        std::string prototype_source_id;
        bool has_composed_overrides = false;
    };

    struct PointInstancer {
        SourceReference source;
        uint32_t node_id = 0;
        std::vector<std::string> prototype_source_ids;
        std::vector<int32_t> proto_indices;
        std::vector<DirectX::XMFLOAT3> positions;
        std::vector<DirectX::XMFLOAT4> orientations;
        std::vector<DirectX::XMFLOAT3> scales;
        std::vector<DirectX::XMFLOAT3> velocities;
        std::vector<DirectX::XMFLOAT3> angular_velocities;
        std::vector<int64_t> ids;
        std::vector<int64_t> invisible_ids;
        std::vector<int64_t> inactive_ids;
        math::AABB local_bounds{};
        uint64_t logical_instance_count = 0;
        std::vector<uint64_t> prototype_instance_counts;
        bool time_varying = false;
    };

    struct ShaderValue {
        SourceReference source;
        std::string name;
        std::string value_type;
        std::string authored_value;
        std::string authored_asset_path;
        std::string resolved_asset_path;
        bool authored = false;
    };

    struct ShaderNode {
        SourceReference source;
        std::string shader_id;
        std::vector<ShaderValue> inputs;
        std::vector<ShaderValue> outputs;
    };

    struct ShaderConnection {
        SourceReference source;
        std::string source_property_path;
        std::string destination_property_path;
    };

    struct MaterialGraph {
        SourceReference source;
        std::vector<std::string> render_contexts;
        std::vector<uint32_t> shader_node_ids;
        std::vector<ShaderConnection> connections;
    };

    struct MaterialBinding {
        SourceReference source;
        std::string material_source_id;
        std::string purpose;
        std::string binding_kind;
        std::string strength;
        std::vector<std::string> collection_targets;
    };

    struct SourceCamera {
        SourceReference source;
        uint32_t node_id = 0;
        std::string projection;
        double focal_length = 0.0;
        double horizontal_aperture = 0.0;
        double vertical_aperture = 0.0;
        double horizontal_aperture_offset = 0.0;
        double vertical_aperture_offset = 0.0;
        double focus_distance = 0.0;
        double f_stop = 0.0;
        double exposure = 0.0;
        DirectX::XMFLOAT2 clipping_range{};
        bool time_varying = false;
    };

    struct SourceLight {
        SourceReference source;
        uint32_t node_id = 0;
        std::string light_type;
        DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        float exposure = 0.0f;
        std::vector<ShaderValue> parameters;
        bool time_varying = false;
    };

    struct SourceAsset {
        SourceReference source;
        std::string authored_path;
        std::string resolved_path;
        std::string category;
        bool resolved = false;
    };

} // namespace scene::source
