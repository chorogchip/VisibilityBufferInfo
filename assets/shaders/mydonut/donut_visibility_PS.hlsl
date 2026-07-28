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

struct PSInput
{
    float4 clipPosition : SV_Position;
    float2 texCoord : TEXCOORD0;
    nointerpolation uint geometryInstanceID : TEXCOORD1;
};

struct PSOutput
{
    uint2 visibility : SV_Target0;
};

PSOutput main(PSInput input, uint primitiveID : SV_PrimitiveID)
{
    if (IsAlphaTestedDomain(g_Material))
    {
        float4 baseTexture = float4(1.0, 1.0, 1.0, 1.0);
        if (HasMaterialFlag(
            g_Material.flags,
            MaterialFlags_UseBaseOrDiffuseTexture))
        {
            baseTexture = t_BaseOrDiffuse.Sample(
                s_MaterialSampler, input.texCoord);
        }

        float opacity = g_Material.opacity;
        if (HasMaterialFlag(
            g_Material.flags,
            MaterialFlags_UseOpacityTexture))
        {
            opacity *= t_Opacity.Sample(s_MaterialSampler, input.texCoord).r;
        }
        else if (HasMaterialFlag(
            g_Material.flags,
            MaterialFlags_UseBaseOrDiffuseTexture))
        {
            opacity *= baseTexture.a;
        }

        clip(saturate(opacity) - g_Material.alphaCutoff);
    }

    PSOutput output;
    output.visibility = uint2(input.geometryInstanceID, primitiveID);
    return output;
}
