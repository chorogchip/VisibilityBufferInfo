#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <DirectXMath.h>

namespace eng {

    struct MaterialCPU {
        using TexturePath = std::optional<std::filesystem::path>;

        std::string name;

        DirectX::XMFLOAT4 base_color{ 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 emissive_color{ 0.0f, 0.0f, 0.0f };
        float emissive_intensity = 1.0f;

        float metalness = 0.0f;
        float roughness = 1.0f;
        float opacity = 1.0f;
        float alpha_cutoff = 0.5f;
        float normal_scale = 1.0f;
        float occlusion_strength = 1.0f;

        TexturePath base_color_texture;
        TexturePath normal_texture;
        TexturePath metal_roughness_texture;
        TexturePath emissive_texture;
        TexturePath occlusion_texture;
        TexturePath opacity_texture;
    };


}
