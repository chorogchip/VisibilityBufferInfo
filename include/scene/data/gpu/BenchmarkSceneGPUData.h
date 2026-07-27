#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <d3d12.h>
#include <DirectXMath.h>

#include "engine/GPUResource.h"
#include "scene/data/cpu/SceneCPUData.h"

namespace scene {

    struct BenchmarkSceneGPUData {
        static constexpr uint32_t MATERIAL_FLAG_BASE_COLOR_TEXTURE = 1u << 0;
        static constexpr uint32_t MATERIAL_FLAG_METAL_ROUGHNESS_TEXTURE = 1u << 1;
        static constexpr uint32_t MATERIAL_FLAG_NORMAL_TEXTURE = 1u << 2;
        static constexpr uint32_t MATERIAL_FLAG_EMISSIVE_TEXTURE = 1u << 3;
        static constexpr uint32_t MATERIAL_FLAG_OCCLUSION_TEXTURE = 1u << 4;
        static constexpr uint32_t MATERIAL_FLAG_ALPHA_TESTED = 1u << 8;
        static constexpr uint32_t MATERIAL_FLAG_DOUBLE_SIDED = 1u << 9;

        struct MeshData {
            uint32_t first_submesh = 0;
            uint32_t submesh_count = 0;
            uint32_t pad0 = 0;
            uint32_t pad1 = 0;
        };

        struct SubmeshData {
            uint32_t vertex_offset = 0;
            uint32_t vertex_count = 0;
            uint32_t index_offset = 0;
            uint32_t index_count = 0;
            uint32_t material_id = 0;
            uint32_t pad0 = 0;
            uint32_t pad1 = 0;
            uint32_t pad2 = 0;
        };

        struct MaterialData {
            DirectX::XMFLOAT4 base_color = { 1.0f, 1.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT3 emissive_color = { 0.0f, 0.0f, 0.0f };
            float emissive_intensity = 1.0f;
            float metalness = 0.0f;
            float roughness = 1.0f;
            float opacity = 1.0f;
            float alpha_cutoff = 0.5f;
            float normal_scale = 1.0f;
            float occlusion_strength = 1.0f;
            uint32_t virtual_shader_id = 0;
            uint32_t flags = 0;
            std::array<uint32_t, 5> texture_indices = {
                source::SceneConstants::INVALID_INDEX,
                source::SceneConstants::INVALID_INDEX,
                source::SceneConstants::INVALID_INDEX,
                source::SceneConstants::INVALID_INDEX,
                source::SceneConstants::INVALID_INDEX
            };
            uint32_t pad1 = 0;
            uint32_t pad2 = 0;
            uint32_t pad3 = 0;
        };

        struct InstanceData {
            uint32_t instance_id = 0;
            uint32_t material_id = 0;
            uint32_t submesh_id = 0;
            uint32_t flags = 0;
            DirectX::XMFLOAT4X4 transform{};
        };

        struct DrawData {
            uint32_t first_instance = 0;
            uint32_t instance_count = 0;
            uint32_t submesh_id = 0;
            uint32_t index_count = 0;
            uint32_t index_offset = 0;
            uint32_t vertex_offset = 0;
            uint32_t material_id = 0;
            uint32_t pad0 = 0;
        };

        eng::GPUResource vertex_buffer;
        eng::GPUResource index_buffer;
        eng::GPUResource mesh_buffer;
        eng::GPUResource submesh_buffer;
        eng::GPUResource material_buffer;
        eng::GPUResource instance_buffer;
        eng::GPUResource render_instance_buffer;
        eng::GPUResource draw_buffer;
        std::vector<eng::GPUResource> textures;

        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
        D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
        std::vector<MaterialData> material_data;
        std::vector<SceneCPUData::DrawCall> draw_calls;

        uint32_t vertex_count = 0;
        uint32_t index_count = 0;
        uint32_t mesh_count = 0;
        uint32_t submesh_count = 0;
        uint32_t material_count = 0;
        uint32_t instance_count = 0;
        uint32_t render_instance_count = 0;
        uint32_t draw_count = 0;

    };

    static_assert(sizeof(BenchmarkSceneGPUData::MeshData) == 16);
    static_assert(sizeof(BenchmarkSceneGPUData::SubmeshData) == 32);
    static_assert(sizeof(BenchmarkSceneGPUData::MaterialData) == 96);
    static_assert(sizeof(BenchmarkSceneGPUData::InstanceData) == 80);
    static_assert(sizeof(BenchmarkSceneGPUData::DrawData) == 32);
}
