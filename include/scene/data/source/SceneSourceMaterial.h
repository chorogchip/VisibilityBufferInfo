#pragma once

#include <cstdint>
#include <string>

#include <DirectXMath.h>

#include "scene/data/source/SceneSourceTexture.h"

namespace scene::source {

    enum class AlphaMode : uint8_t {
        Opaque,
        Mask,
        Blend
    };

    struct Material {
        std::string name;
        DirectX::XMFLOAT4 base_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 emissive_color = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 specular_color = { 1.0f, 1.0f, 1.0f };
        float emissive_intensity = 1.0f;

        float metalness = 0.0f;
        float roughness = 1.0f;
        float alpha_cutoff = 0.5f;
        float normal_scale = 1.0f;
        float occlusion_strength = 1.0f;
        float transmission = 0.0f;
        float specular = 1.0f;
        float ior = 1.5f;

        AlphaMode alpha_mode = AlphaMode::Opaque;
        bool double_sided = false;
        uint32_t virtual_shader_id = 0;

        TextureRef base_color_texture;
        TextureRef metal_roughness_texture;
        TextureRef normal_texture;
        TextureRef emissive_texture;
        TextureRef occlusion_texture;
        TextureRef transmission_texture;
    };
}
