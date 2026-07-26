#ifndef COMMON_MATERIAL_DATA_HLSLI
#define COMMON_MATERIAL_DATA_HLSLI

struct MaterialData
{
    float4 base_color;

    float3 emissive_color;
    float emissive_intensity;

    float metalness;
    float roughness;
    float opacity;
    float alpha_cutoff;

    float normal_scale;
    float occlusion_strength;
    uint flags;
    uint padding0;

    uint texture_indices[5];
    uint padding1;
    uint padding2;
    uint padding3;
};

#endif
