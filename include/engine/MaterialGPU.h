#pragma once

#include <cstdint>

#include <DirectXMath.h>

namespace eng {

    struct MaterialGPU {
        static constexpr uint32_t invalid_texture_index = UINT32_MAX;

        DirectX::XMFLOAT4 base_color{ 1.0f, 1.0f, 1.0f, 1.0f };

        DirectX::XMFLOAT3 emissive_color{ 0.0f, 0.0f, 0.0f };
        float emissive_intensity = 1.0f;

        float metalness = 0.0f;
        float roughness = 1.0f;
        float opacity = 1.0f;
        float alpha_cutoff = 0.5f;

        float normal_scale = 1.0f;
        float occlusion_strength = 1.0f;
        uint32_t flags = 0;
        uint32_t padding0 = 0;

        uint32_t base_color_texture = invalid_texture_index;
        uint32_t normal_texture = invalid_texture_index;
        uint32_t metal_roughness_texture = invalid_texture_index;
        uint32_t emissive_texture = invalid_texture_index;

        uint32_t occlusion_texture = invalid_texture_index;
        uint32_t opacity_texture = invalid_texture_index;
        uint32_t padding1 = 0;
        uint32_t padding2 = 0;
    };

    static_assert(sizeof(MaterialGPU) == 96);

}
