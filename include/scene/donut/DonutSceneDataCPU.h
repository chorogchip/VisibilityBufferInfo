#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "scene/donut/SceneStructures.h"

namespace scene {

    inline constexpr uint32_t invalid_index = static_cast<uint32_t>(-1);
    inline constexpr int32_t invalid_descriptor_index = -1;

    enum class MaterialTextureSlot : uint32_t {
        base_or_diffuse = 0,
        metal_rough_or_specular = 1,
        normal = 2,
        emissive = 3,
        occlusion = 4,
        transmission = 5,
        opacity = 6,
        count = 7
    };

    inline constexpr size_t material_texture_slot_count =
        static_cast<size_t>(MaterialTextureSlot::count);

    inline constexpr int32_t material_domain_opaque = 0;
    inline constexpr int32_t material_domain_alpha_tested = 1;
    inline constexpr int32_t material_domain_alpha_blended = 2;
    inline constexpr int32_t material_domain_transmissive = 3;

    inline constexpr int32_t material_flag_use_specular_gloss = 0x00000001;
    inline constexpr int32_t material_flag_double_sided = 0x00000002;
    inline constexpr int32_t material_flag_use_metal_rough_or_specular = 0x00000004;
    inline constexpr int32_t material_flag_use_base_or_diffuse = 0x00000008;
    inline constexpr int32_t material_flag_use_emissive = 0x00000010;
    inline constexpr int32_t material_flag_use_normal = 0x00000020;
    inline constexpr int32_t material_flag_use_occlusion = 0x00000040;
    inline constexpr int32_t material_flag_use_transmission = 0x00000080;
    inline constexpr int32_t material_flag_use_opacity = 0x00000200;

    struct DonutSceneDataCPU {
        using TexturePath = std::optional<std::filesystem::path>;

        struct Geometry {
            std::string name;
            uint32_t vertex_offset = 0;
            uint32_t vertex_count = 0;
            uint32_t index_offset = 0;
            uint32_t index_count = 0;
            uint32_t material_index = 0;
        };

        struct Mesh {
            std::string name;
            uint32_t first_geometry = 0;
            uint32_t geometry_count = 0;
        };

        struct Instance {
            uint32_t mesh_index = 0;
            uint32_t first_geometry_instance = 0;
            DirectX::XMFLOAT3X4 transform{};
            DirectX::XMFLOAT3X4 prev_transform{};
        };

        struct Draw {
            uint32_t geometry_index = 0;
            uint32_t instance_index = 0;
        };

        struct Material {
            std::string name;
            dnt::MaterialConstants constants{};
            std::array<TexturePath, material_texture_slot_count> textures{};
        };

        std::filesystem::path source_path;
        bool loaded = false;
        std::string error_message;

        std::vector<DirectX::XMFLOAT3> positions;
        std::vector<DirectX::XMFLOAT3> prev_positions;
        std::vector<DirectX::XMFLOAT2> texcoords;
        std::vector<uint32_t> packed_normals;
        std::vector<uint32_t> packed_tangents;
        std::vector<uint32_t> indices;

        std::vector<Geometry> geometries;
        std::vector<Mesh> meshes;
        std::vector<Instance> instances;
        std::vector<Draw> draws;
        std::vector<Material> materials;

        bool validate(std::string& error_message) const;
    };
}
