#include "donut_gbuffer_common.hlsli"

Texture2D t_BaseOrDiffuse :
    register(MATERIAL_BASE_COLOR_REGISTER, MATERIAL_SPACE);
Texture2D t_Opacity :
    register(MATERIAL_OPACITY_REGISTER, MATERIAL_SPACE);

SamplerState s_MaterialSampler :
    register(GBUFFER_MATERIAL_SAMPLER_REGISTER, GBUFFER_VIEW_SPACE);

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
    if (IsAlphaTestedDomain())
    {
        float4 baseTexture = float4(1.0, 1.0, 1.0, 1.0);
        if (HasMaterialFlag(MaterialFlags_UseBaseOrDiffuseTexture))
        {
            baseTexture = t_BaseOrDiffuse.Sample(
                s_MaterialSampler, input.texCoord);
        }

        float opacity = g_Material.opacity;
        if (HasMaterialFlag(MaterialFlags_UseOpacityTexture))
        {
            opacity *= t_Opacity.Sample(s_MaterialSampler, input.texCoord).r;
        }
        else if (HasMaterialFlag(MaterialFlags_UseBaseOrDiffuseTexture))
        {
            opacity *= baseTexture.a;
        }

        clip(saturate(opacity) - g_Material.alphaCutoff);
    }

    PSOutput output;
    output.visibility = uint2(input.geometryInstanceID, primitiveID);
    return output;
}
