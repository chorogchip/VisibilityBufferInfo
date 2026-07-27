#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <d3d12.h>
#include <DirectXMath.h>

#include "engine/GPUResource.h"
#include "scene/data/source/SceneConstants.h"

namespace scene {

    struct DonutSceneGPUData {
        static constexpr uint32_t MATERIAL_FLAG_BASE_COLOR_TEXTURE = 1u << 0;
        static constexpr uint32_t MATERIAL_FLAG_METAL_ROUGHNESS_TEXTURE = 1u << 1;
        static constexpr uint32_t MATERIAL_FLAG_NORMAL_TEXTURE = 1u << 2;
        static constexpr uint32_t MATERIAL_FLAG_EMISSIVE_TEXTURE = 1u << 3;
        static constexpr uint32_t MATERIAL_FLAG_OCCLUSION_TEXTURE = 1u << 4;
        static constexpr uint32_t MATERIAL_FLAG_DOUBLE_SIDED = 1u << 8;
        static constexpr uint32_t MATERIAL_FLAG_ALPHA_TESTED = 1u << 9;

        static constexpr uint32_t MATERIAL_TEXTURE_DESCRIPTOR_COUNT = 7;
        static constexpr uint32_t MAX_MATERIAL_TEXTURE_DESCRIPTOR_COUNT = 4096;

        static constexpr int32_t SHADER_MATERIAL_DOMAIN_OPAQUE = 0;
        static constexpr int32_t SHADER_MATERIAL_DOMAIN_ALPHA_TESTED = 1;
        static constexpr int32_t SHADER_MATERIAL_FLAG_DOUBLE_SIDED = 0x00000002;
        static constexpr int32_t SHADER_MATERIAL_FLAG_USE_METAL_ROUGHNESS_TEXTURE = 0x00000004;
        static constexpr int32_t SHADER_MATERIAL_FLAG_USE_BASE_COLOR_TEXTURE = 0x00000008;
        static constexpr int32_t SHADER_MATERIAL_FLAG_USE_EMISSIVE_TEXTURE = 0x00000010;
        static constexpr int32_t SHADER_MATERIAL_FLAG_USE_NORMAL_TEXTURE = 0x00000020;
        static constexpr int32_t SHADER_MATERIAL_FLAG_USE_OCCLUSION_TEXTURE = 0x00000040;

        struct VertexLayout {
            uint32_t position_offset = 0;
            uint32_t prev_position_offset = 0;
            uint32_t texcoord_offset = 0;
            uint32_t normal_offset = 0;
            uint32_t tangent_offset = 0;
            uint32_t byte_size = 0;
        };

        struct InstanceData {
            uint32_t flags = 0;
            uint32_t first_geometry_instance = 0;
            uint32_t first_geometry = 0;
            uint32_t geometry_instance_count = 0;
            DirectX::XMFLOAT3X4 transform{};
            DirectX::XMFLOAT3X4 prev_transform{};
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

        struct GeometryInstanceData {
            uint32_t instance_id = 0;
            uint32_t submesh_id = 0;
            uint32_t pad0 = 0;
            uint32_t pad1 = 0;
        };

        struct DrawInstanceData {
            uint32_t instance_id = 0;
            uint32_t submesh_id = 0;
        };

        struct MaterialData {
            DirectX::XMFLOAT4 base_color = { 1.0f, 1.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT3 emissive_color = { 0.0f, 0.0f, 0.0f };
            float roughness = 1.0f;
            float metalness = 0.0f;
            float normal_scale = 1.0f;
            float occlusion_strength = 1.0f;
            float alpha_cutoff = 0.5f;
            uint32_t virtual_shader_id = 0;
            uint32_t flags = 0;
            int32_t domain = SHADER_MATERIAL_DOMAIN_OPAQUE;
            std::array<uint32_t, MATERIAL_TEXTURE_DESCRIPTOR_COUNT>
                texture_indices = [] {
                    std::array<
                        uint32_t,
                        MATERIAL_TEXTURE_DESCRIPTOR_COUNT> indices{};
                    indices.fill(source::SceneConstants::INVALID_INDEX);
                    return indices;
                }();
            uint32_t pad0 = 0;
            uint32_t pad1 = 0;
        };

        struct ShaderMaterialConstants {
            DirectX::XMFLOAT3 base_or_diffuse_color = { 1.0f, 1.0f, 1.0f };
            int32_t flags = 0;
            DirectX::XMFLOAT3 specular_color = { 0.04f, 0.04f, 0.04f };
            int32_t material_id = 0;
            DirectX::XMFLOAT3 emissive_color = { 0.0f, 0.0f, 0.0f };
            int32_t domain = SHADER_MATERIAL_DOMAIN_OPAQUE;
            float opacity = 1.0f;
            float roughness = 1.0f;
            float metalness = 0.0f;
            float normal_texture_scale = 1.0f;
            float occlusion_strength = 1.0f;
            float alpha_cutoff = 0.5f;
            float transmission_factor = 0.0f;
            int32_t base_or_diffuse_texture_index = 0;
            int32_t metal_rough_or_specular_texture_index = 0;
            int32_t emissive_texture_index = 0;
            int32_t normal_texture_index = 0;
            int32_t occlusion_texture_index = 0;
            int32_t transmission_texture_index = 0;
            int32_t opacity_texture_index = 0;
            DirectX::XMFLOAT2 normal_texture_transform_scale = { 1.0f, 1.0f };
            uint32_t padding1[3]{};
            float sss_scale = 0.0f;
            DirectX::XMFLOAT3 sss_transmission_color = { 0.0f, 0.0f, 0.0f };
            float sss_anisotropy = 0.0f;
            DirectX::XMFLOAT3 sss_scattering_color = { 0.0f, 0.0f, 0.0f };
            float hair_melanin = 0.0f;
            DirectX::XMFLOAT3 hair_base_color = { 0.0f, 0.0f, 0.0f };
            float hair_melanin_redness = 0.0f;
            float hair_longitudinal_roughness = 0.0f;
            float hair_azimuthal_roughness = 0.0f;
            float hair_ior = 0.0f;
            float hair_cuticle_angle = 0.0f;
            DirectX::XMFLOAT3 hair_diffuse_reflection_tint = { 0.0f, 0.0f, 0.0f };
            float hair_diffuse_reflection_weight = 0.0f;
        };

        eng::GPUResource vertex_buffer;
        eng::GPUResource index_buffer;
        eng::GPUResource instance_buffer;
        eng::GPUResource draw_instance_buffer;
        eng::GPUResource draw_instance_id_buffer;
        eng::GPUResource submesh_buffer;
        eng::GPUResource geometry_instance_buffer;
        eng::GPUResource material_buffer;
        eng::GPUResource material_constant_buffer;
        std::vector<eng::GPUResource> textures;

        D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
        VertexLayout vertex_layout{};
        uint32_t vertex_count = 0;
        uint32_t index_count = 0;
        uint32_t material_constant_stride =
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        uint32_t draw_instance_id_capacity = 0;
        std::array<uint32_t, 3> fallback_texture_indices{};

        std::vector<InstanceData> instance_data;
        std::vector<DrawInstanceData> draw_instance_data;
        std::vector<SubmeshData> submesh_data;
        std::vector<GeometryInstanceData> geometry_instance_data;
        std::vector<MaterialData> material_data;

    };

    static_assert(sizeof(DonutSceneGPUData::InstanceData) == 112);
    static_assert(sizeof(DonutSceneGPUData::SubmeshData) == 32);
    static_assert(sizeof(DonutSceneGPUData::GeometryInstanceData) == 16);
    static_assert(sizeof(DonutSceneGPUData::DrawInstanceData) == 8);
    static_assert(sizeof(DonutSceneGPUData::MaterialData) == 96);
    static_assert(sizeof(DonutSceneGPUData::ShaderMaterialConstants) == 208);
    static_assert(offsetof(DonutSceneGPUData::InstanceData, transform) == 16);
    static_assert(offsetof(DonutSceneGPUData::SubmeshData, material_id) == 16);
    static_assert(offsetof(DonutSceneGPUData::MaterialData, texture_indices) == 60);
    static_assert(offsetof(
        DonutSceneGPUData::ShaderMaterialConstants,
        material_id) == 28);
    static_assert(offsetof(
        DonutSceneGPUData::ShaderMaterialConstants,
        base_or_diffuse_texture_index) == 76);
}
