#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include <DirectXMath.h>

#include "math/AABB.h"
#include "scene/data/source/SceneConstants.h"
#include "scene/data/source/SceneSourceMaterial.h"

namespace scene {

    struct SceneCPUData {
        struct Vertex {
            DirectX::XMFLOAT3 position{};
            DirectX::XMFLOAT3 normal{};
            DirectX::XMFLOAT4 tangent{};
            DirectX::XMFLOAT2 uv0{};
        };

        struct Material {
            using TexturePath = std::optional<std::filesystem::path>;

            DirectX::XMFLOAT4 base_color = { 1.0f, 1.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT3 emissive_color = { 0.0f, 0.0f, 0.0f };
            float emissive_intensity = 1.0f;
            float metalness = 0.0f;
            float roughness = 1.0f;
            float opacity = 1.0f;
            float alpha_cutoff = 0.5f;
            float normal_scale = 1.0f;
            float occlusion_strength = 1.0f;
            source::AlphaMode alpha_mode = source::AlphaMode::Opaque;
            bool double_sided = false;
            uint32_t virtual_shader_id = 0;
            TexturePath base_color_texture;
            TexturePath metal_roughness_texture;
            TexturePath normal_texture;
            TexturePath emissive_texture;
            TexturePath occlusion_texture;
        };

        struct Submesh {
            uint32_t vertex_offset = 0;
            uint32_t vertex_count = 0;
            uint32_t index_offset = 0;
            uint32_t index_count = 0;
            uint32_t material_id = 0;
            math::AABB local_aabb{};
        };

        struct Mesh {
            uint32_t first_submesh = 0;
            uint32_t submesh_count = 0;
            math::AABB local_aabb{};
        };

        struct Instance {
            uint32_t mesh_id = source::SceneConstants::INVALID_INDEX;
            DirectX::XMFLOAT4X4 world_transform{};
            math::AABB world_aabb{};
        };

        struct Node {
            std::vector<uint32_t> children;
            DirectX::XMFLOAT4X4 local_transform{};
            DirectX::XMFLOAT4X4 world_transform{};
            uint32_t mesh_id = source::SceneConstants::INVALID_INDEX;
            uint32_t first_instance = 0;
            uint32_t instance_count = 0;
            math::AABB subtree_world_aabb{};
        };

        struct DrawCall {
            uint32_t first_instance = 0;
            uint32_t instance_count = 0;
            uint32_t submesh_id = 0;
            uint32_t index_count = 0;
            uint32_t index_offset = 0;
            uint32_t vertex_offset = 0;
            uint32_t material_id = 0;
        };

        struct DrawInstance {
            uint32_t instance_id = 0;
            uint32_t submesh_id = 0;
        };

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<Material> materials;
        std::vector<Submesh> submeshes;
        std::vector<Mesh> meshes;
        uint32_t root_node_id = source::SceneConstants::INVALID_INDEX;
        std::vector<Node> nodes;
        std::vector<Instance> instances;
        std::vector<DrawInstance> draw_instances;
        std::vector<DrawCall> draw_calls;
        math::AABB world_aabb{};

    };

    static_assert(sizeof(SceneCPUData::Vertex) == 48);
    static_assert(sizeof(SceneCPUData::DrawInstance) == 8);
}
