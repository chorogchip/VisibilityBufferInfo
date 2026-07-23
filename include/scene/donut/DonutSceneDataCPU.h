#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <DirectXCollision.h>
#include <DirectXMath.h>

#include "math/AABB.h"

namespace scene {

    struct DonutSceneDataCPU {

        static constexpr uint32_t INVALID_INDEX = static_cast<uint32_t>(-1);

        // Matches the Donut material SRV register order t0..t6.
        enum class EnumMaterialTextureSlot : uint32_t {
            BASE_COLOR,
            METAL_ROUGHNESS,
            NORMAL,
            EMISSIVE,
            OCCLUSION,
            TRANSMISSION,
            OPACITY,
            COUNT
        };
        enum class EnumMaterialDomain : uint32_t {
            Opaque,
            AlphaTested,
            AlphaBlended
        };
        enum class EnumTextureColorSpace : uint32_t { LINEAR, SRGB };
        enum class EnumTextureFallback : uint32_t { WHITE, BLACK, FLAT_NORMAL };
        static constexpr size_t MATERIAL_TEXTURE_SLOT_COUNT =
            static_cast<size_t>(EnumMaterialTextureSlot::COUNT);

        struct Texture {
            std::string name;
            std::filesystem::path path;
            EnumTextureColorSpace color_space = EnumTextureColorSpace::LINEAR;
            EnumTextureFallback fallback = EnumTextureFallback::WHITE;
            bool embedded = false;
        };

        struct Material {
            std::string name;
            DirectX::XMFLOAT4 base_color = { 1.0f, 1.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT3 emissive_color = { 0.0f, 0.0f, 0.0f };
            float roughness = 1.0f;
            float metalness = 0.0f;
            float normal_scale = 1.0f;
            float occlusion_strength = 1.0f;
            float alpha_cutoff = 0.5f;
            EnumMaterialDomain domain = EnumMaterialDomain::Opaque;
            bool double_sided = false;
            std::array<uint32_t, MATERIAL_TEXTURE_SLOT_COUNT> texture_ids = [] {
                std::array<uint32_t, MATERIAL_TEXTURE_SLOT_COUNT> ids{};
                ids.fill(INVALID_INDEX);
                return ids;
            }();
        };

        struct Submesh {
            std::string name;
            uint32_t vertex_offset = 0;
            uint32_t vertex_count = 0;
            uint32_t index_offset = 0;
            uint32_t index_count = 0;
            uint32_t material_id = 0;
            math::AABB local_aabb{};
        };

        struct Mesh {
            std::string name;
            uint32_t first_submesh = 0;
            uint32_t submesh_count = 0;
            math::AABB local_aabb{};
        };

        struct GeometryInstance {
            uint32_t instance_id = 0;
            uint32_t submesh_id = 0;
        };

        struct Instance {
            uint32_t node_id = INVALID_INDEX;
            uint32_t mesh_id = 0;
            uint32_t first_geometry_instance = 0;
            uint32_t geometry_instance_count = 0;
            DirectX::XMFLOAT4X4 world_transform{};
            DirectX::XMFLOAT4X4 prev_world_transform{};
            math::AABB world_aabb{};
        };

        struct Node {
            std::string name;
            uint32_t parent_id = INVALID_INDEX;
            uint32_t instance_id = INVALID_INDEX;
            std::vector<uint32_t> children;
            DirectX::XMFLOAT4X4 local_transform{};
            DirectX::XMFLOAT4X4 world_transform{};
            math::AABB subtree_world_aabb{};
        };

        struct VisibleDraw {
            uint32_t geometry_instance_id = 0;
        };

        std::filesystem::path source_path;
        bool loaded = false;
        std::string error_message;
        uint32_t root_node_id = INVALID_INDEX;
        math::AABB world_aabb{};

        std::vector<Node> nodes;
        std::vector<Instance> instances;
        std::vector<Mesh> meshes;
        std::vector<Submesh> submeshes;
        std::vector<GeometryInstance> geometry_instances;
        std::vector<Material> materials;
        std::vector<Texture> textures;

        std::vector<DirectX::XMFLOAT3> positions;
        std::vector<DirectX::XMFLOAT3> prev_positions;
        std::vector<DirectX::XMFLOAT2> texcoords;
        std::vector<uint32_t> packed_normals;
        std::vector<uint32_t> packed_tangents;
        std::vector<uint32_t> indices;

        bool validate(std::string& error_message) const;
        void build_all_visible_draws(std::vector<VisibleDraw>& output) const;
        void build_visible_draws(
            std::vector<VisibleDraw>& output,
            const DirectX::BoundingFrustum* frustum) const;
        void sort_visible_draws(std::vector<VisibleDraw>& draws) const;
    };
}
