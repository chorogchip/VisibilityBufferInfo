#include "donut_gbuffer_common.hlsli"

Texture2D t_BaseOrDiffuse :
    register(MATERIAL_BASE_COLOR_REGISTER, MATERIAL_SPACE);
Texture2D t_Opacity :
    register(MATERIAL_OPACITY_REGISTER, MATERIAL_SPACE);

SamplerState s_MaterialSampler :
    register(GBUFFER_MATERIAL_SAMPLER_REGISTER, GBUFFER_VIEW_SPACE);

void main(float4 clipPosition : SV_Position, float2 texCoord : TEXCOORD)
{
    if (!IsAlphaTestedDomain())
        return;

    float opacity = g_Material.opacity;
    if (HasMaterialFlag(MaterialFlags_UseOpacityTexture))
    {
        opacity *= t_Opacity.Sample(s_MaterialSampler, texCoord).r;
    }
    else if (HasMaterialFlag(MaterialFlags_UseBaseOrDiffuseTexture))
    {
        opacity *= t_BaseOrDiffuse.Sample(s_MaterialSampler, texCoord).a;
    }

    clip(saturate(opacity) - g_Material.alphaCutoff);
}
