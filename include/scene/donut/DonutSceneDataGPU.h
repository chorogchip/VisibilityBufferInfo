#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>

#include "scene/donut/DonutSceneDataCPU.h"

namespace scene {

    struct DonutSceneDataGPU {
        static constexpr uint32_t MATERIAL_FLAG_BASE_COLOR_TEXTURE = 1u << 0;
        static constexpr uint32_t MATERIAL_FLAG_METAL_ROUGHNESS_TEXTURE = 1u << 1;
        static constexpr uint32_t MATERIAL_FLAG_NORMAL_TEXTURE = 1u << 2;
        static constexpr uint32_t MATERIAL_FLAG_EMISSIVE_TEXTURE = 1u << 3;
        static constexpr uint32_t MATERIAL_FLAG_OCCLUSION_TEXTURE = 1u << 4;
        static constexpr uint32_t MATERIAL_FLAG_DOUBLE_SIDED = 1u << 8;
        static constexpr int32_t SHADER_MATERIAL_DOMAIN_OPAQUE = 0;
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

        struct MaterialData {
            DirectX::XMFLOAT4 base_color = { 1.0f, 1.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT3 emissive_color = { 0.0f, 0.0f, 0.0f };
            float roughness = 1.0f;
            float metalness = 0.0f;
            float normal_scale = 1.0f;
            float occlusion_strength = 1.0f;
            uint32_t flags = 0;
            std::array<uint32_t, DonutSceneDataCPU::MATERIAL_TEXTURE_SLOT_COUNT> texture_indices{};
            uint32_t pad0 = 0;
            uint32_t pad1 = 0;
            uint32_t pad2 = 0;
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

        struct Draw {
            uint32_t geometry_instance_id = 0;
            uint32_t instance_id = 0;
            uint32_t submesh_id = 0;
            uint32_t index_count = 0;
            uint32_t index_offset = 0;
            uint32_t vertex_offset = 0;
            uint32_t material_id = 0;
        };

        Microsoft::WRL::ComPtr<ID3D12Resource> vertex_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> index_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> instance_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> submesh_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> geometry_instance_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> material_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> material_constant_buffer;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> textures;

        D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
        VertexLayout vertex_layout{};
        uint32_t material_constant_stride = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        std::array<uint32_t, 3> fallback_texture_indices{};

        std::vector<InstanceData> instance_data;
        std::vector<SubmeshData> submesh_data;
        std::vector<GeometryInstanceData> geometry_instance_data;
        std::vector<MaterialData> material_data;
        std::vector<Draw> draws;
    };

    static_assert(sizeof(DonutSceneDataGPU::InstanceData) == 112);
    static_assert(sizeof(DonutSceneDataGPU::SubmeshData) == 32);
    static_assert(sizeof(DonutSceneDataGPU::GeometryInstanceData) == 16);
    static_assert(sizeof(DonutSceneDataGPU::MaterialData) == 80);
    static_assert(sizeof(DonutSceneDataGPU::ShaderMaterialConstants) == 208);
    static_assert(offsetof(DonutSceneDataGPU::InstanceData, transform) == 16);
    static_assert(offsetof(DonutSceneDataGPU::SubmeshData, material_id) == 16);
    static_assert(offsetof(DonutSceneDataGPU::MaterialData, texture_indices) == 48);
    static_assert(offsetof(DonutSceneDataGPU::ShaderMaterialConstants, material_id) == 28);
    static_assert(offsetof(DonutSceneDataGPU::ShaderMaterialConstants, base_or_diffuse_texture_index) == 76);
}
