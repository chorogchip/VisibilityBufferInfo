#include "..\common\mydonut_scene_abi.hlsli"

cbuffer c_Material : register(b0, space0)
{
    MaterialConstants g_Material;
};

Texture2D t_BaseOrDiffuse :
    register(t0, space0);
Texture2D t_Opacity :
    register(t6, space0);

SamplerState s_MaterialSampler :
    register(s0, space2);

void main(float4 clipPosition : SV_Position, float2 texCoord : TEXCOORD)
{
    if (!IsAlphaTestedDomain(g_Material))
        return;

    float opacity = g_Material.opacity;
    if (HasMaterialFlag(
        g_Material.flags,
        MaterialFlags_UseOpacityTexture))
    {
        opacity *= t_Opacity.Sample(s_MaterialSampler, texCoord).r;
    }
    else if (HasMaterialFlag(
        g_Material.flags,
        MaterialFlags_UseBaseOrDiffuseTexture))
    {
        opacity *= t_BaseOrDiffuse.Sample(s_MaterialSampler, texCoord).a;
    }

    clip(saturate(opacity) - g_Material.alphaCutoff);
}
