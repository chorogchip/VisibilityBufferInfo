#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace scene::raw {

    enum RawJunglePrimState : uint32_t {
        RawJunglePrimStateActive = 1u << 0u,
        RawJunglePrimStateLoaded = 1u << 1u,
        RawJunglePrimStateDefined = 1u << 2u,
        RawJunglePrimStateAbstract = 1u << 3u,
        RawJunglePrimStateNativeInstance = 1u << 4u,
        RawJunglePrimStatePrototype = 1u << 5u,
        RawJunglePrimStatePointInstancer = 1u << 6u,
    };

    struct RawJungleLayerInfo {
        std::string identifier;
        std::string resolved_path;
        bool used_by_stage = false;
        bool is_root_layer = false;
    };

    struct RawJungleAssetInfo {
        std::string source_property_path;
        std::string authored_path;
        std::string resolved_path;
        std::string category;
        bool resolved = false;
    };

    struct RawJungleInventoryEntry {
        std::string relative_path;
        std::string category;
        uint64_t byte_count = 0;
        bool used_by_stage = false;
    };

    struct RawJungleDiagnostic {
        std::string severity;
        std::string message;
        std::string source_file;
        uint64_t source_line = 0;
    };

    struct RawJungleVariantSelection {
        std::string prim_path;
        std::string set_name;
        std::string selection;
        std::vector<std::string> variants;
    };

    struct RawJunglePrimInfo {
        std::string path;
        std::string type_name;
        std::string native_prototype_type;
        std::vector<std::string> applied_schemas;
        std::vector<std::string> authored_metadata_keys;
        std::vector<std::string> custom_data_keys;
        std::vector<std::string> prim_stack_layers;
        uint32_t state_flags = 0;
        uint64_t property_begin = 0;
        uint64_t property_count = 0;
    };

    struct RawJunglePropertyInfo {
        std::string path;
        std::string kind;
        std::string type_name;
        std::string resolved_value_source;
        std::vector<std::string> authored_metadata_keys;
        std::vector<std::string> custom_data_keys;
        std::vector<std::string> property_stack_layers;
        std::vector<std::string> connections_or_targets;
        uint64_t time_sample_count = 0;
        bool authored = false;
        bool has_value_clip = false;
    };

    struct RawJungleMaterialBindingInfo {
        std::string prim_path;
        std::string purpose;
        std::string binding_kind;
        std::string strength;
        std::string relationship_path;
        std::vector<std::string> targets;
    };

    struct RawJunglePointInstancerInfo {
        std::string prim_path;
        std::vector<std::string> prototype_targets;
        std::string proto_indices_property_path;
        uint64_t proto_indices_time_sample_count = 0;
        std::vector<std::string> instance_attribute_paths;
    };

    struct RawJungleStatistics {
        uint64_t prim_count = 0;
        uint64_t property_count = 0;
        uint64_t authored_property_count = 0;
        uint64_t attribute_count = 0;
        uint64_t relationship_count = 0;
        uint64_t property_time_sample_count = 0;
        uint64_t value_clip_property_count = 0;
        uint64_t native_instance_count = 0;
        uint64_t prototype_count = 0;
        uint64_t prototype_prim_count = 0;
        uint64_t prototype_property_count = 0;
        uint64_t point_instancer_count = 0;
        uint64_t mesh_count = 0;
        uint64_t primvar_count = 0;
        uint64_t material_count = 0;
        uint64_t shader_count = 0;
        uint64_t node_graph_count = 0;
        uint64_t camera_count = 0;
        uint64_t light_count = 0;
        uint64_t render_settings_count = 0;
        uint64_t unknown_schema_count = 0;
        std::map<std::string, uint64_t> prim_type_counts;
        std::map<std::string, uint64_t> prototype_type_counts;
        std::map<std::string, uint64_t> applied_schema_counts;
        std::map<std::string, uint64_t> shader_id_counts;
    };

    struct RawJungleStageInfo {
        std::string root_layer_identifier;
        std::string root_layer_resolved_path;
        std::string default_prim_path;
        std::string up_axis;
        double meters_per_unit = 0.0;
        double start_time_code = 0.0;
        double end_time_code = 0.0;
        double frames_per_second = 0.0;
        double time_codes_per_second = 0.0;
        std::string resolver_context;
        std::vector<std::string> load_rules;
        std::map<std::string, std::string> root_layer_metadata;
    };

    // Owns a LoadAll USD stage and records only metadata/index information.
    // Geometry, property values, time samples, materials, and layer specs stay
    // in OpenUSD and are queried from the retained raw document on demand.
    class SceneRawJungle {
    public:
        static std::unique_ptr<SceneRawJungle> open(
            const std::filesystem::path& root_path);

        ~SceneRawJungle();
        SceneRawJungle(SceneRawJungle&&) noexcept;
        SceneRawJungle& operator=(SceneRawJungle&&) noexcept;
        SceneRawJungle(const SceneRawJungle&) = delete;
        SceneRawJungle& operator=(const SceneRawJungle&) = delete;

        const std::filesystem::path& root_path() const;
        const RawJungleStageInfo& stage_info() const;
        const RawJungleStatistics& statistics() const;
        const std::vector<RawJungleLayerInfo>& layers() const;
        const std::vector<RawJungleInventoryEntry>& inventory() const;
        const std::vector<RawJungleAssetInfo>& asset_references() const;
        const std::vector<RawJungleDiagnostic>& diagnostics() const;
        const std::vector<RawJungleVariantSelection>& variant_selections() const;
        const std::vector<RawJunglePrimInfo>& prims() const;
        const std::vector<RawJunglePropertyInfo>& properties() const;
        const std::vector<RawJungleMaterialBindingInfo>& material_bindings() const;
        const std::vector<RawJunglePointInstancerInfo>& point_instancers() const;

        void write_manifest(const std::filesystem::path& output_path) const;

    private:
        class Impl;
        friend class SceneRawJungleUsdAccess;

        explicit SceneRawJungle(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> impl_;
    };

} // namespace scene::raw
