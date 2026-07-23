#include "donut_gbuffer_common.hlsli"

Texture2D t_BaseOrDiffuse :
    register(MATERIAL_BASE_COLOR_REGISTER, MATERIAL_SPACE);
Texture2D t_MetalRoughOrSpecular :
    register(MATERIAL_METAL_ROUGHNESS_REGISTER, MATERIAL_SPACE);
Texture2D t_Normal :
    register(MATERIAL_NORMAL_REGISTER, MATERIAL_SPACE);
Texture2D t_Emissive :
    register(MATERIAL_EMISSIVE_REGISTER, MATERIAL_SPACE);
Texture2D t_Occlusion :
    register(MATERIAL_OCCLUSION_REGISTER, MATERIAL_SPACE);
Texture2D t_Transmission :
    register(MATERIAL_TRANSMISSION_REGISTER, MATERIAL_SPACE);
Texture2D t_Opacity :
    register(MATERIAL_OPACITY_REGISTER, MATERIAL_SPACE);

SamplerState s_MaterialSampler :
    register(GBUFFER_MATERIAL_SAMPLER_REGISTER, GBUFFER_VIEW_SPACE);

struct PSInput
{
    float4 clipPosition : SV_Position;
    float2 texCoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 tangent : TANGENT0;
};

struct PSOutput
{
    float4 channel0 : SV_Target0;
    float4 channel1 : SV_Target1;
    float4 channel2 : SV_Target2;
    float4 channel3 : SV_Target3;
};

float3 ApplyNormalMap(float3 normal, float4 tangent, float4 normalTexture)
{
    const float tangentLengthSq = dot(tangent.xyz, tangent.xyz);
    const bool hasValidTangent = tangentLengthSq > 0.0 && tangent.w != 0.0;

    float3 textureNormal = normalTexture.xyz;
    textureNormal.xy = textureNormal.xy * 2.0 - 1.0;
    textureNormal.xy *= g_Material.normalTextureScale;
    if (textureNormal.z <= 0.0)
    {
        textureNormal.z = sqrt(saturate(
            1.0 - textureNormal.x * textureNormal.x -
            textureNormal.y * textureNormal.y));
    }
    else
    {
        textureNormal.z = abs(textureNormal.z * 2.0 - 1.0);
    }

    const float normalMapLengthSq = dot(textureNormal, textureNormal);
    const bool hasValidNormalMap = normalMapLengthSq > 0.0;
    const float3 localNormal =
        textureNormal * rsqrt(max(normalMapLengthSq, 1e-20));
    const float3 tangentUnit =
        tangent.xyz * rsqrt(max(tangentLengthSq, 1e-20));
    const float3 bitangent = cross(normal, tangentUnit) * tangent.w;
    const float3 mappedNormal = normalize(
        tangentUnit * localNormal.x +
        bitangent * localNormal.y +
        normal * localNormal.z);
    return (hasValidTangent && hasValidNormalMap) ? mappedNormal : normal;
}

PSOutput main(PSInput input, bool isFrontFace : SV_IsFrontFace)
{
    float4 baseTexture = float4(1.0, 1.0, 1.0, 1.0);
    if (HasMaterialFlag(MaterialFlags_UseBaseOrDiffuseTexture))
        baseTexture = t_BaseOrDiffuse.Sample(s_MaterialSampler, input.texCoord);

    float4 metalRoughnessTexture = float4(1.0, 1.0, 1.0, 1.0);
    if (HasMaterialFlag(MaterialFlags_UseMetalRoughOrSpecularTexture))
    {
        metalRoughnessTexture =
            t_MetalRoughOrSpecular.Sample(s_MaterialSampler, input.texCoord);
    }

    float occlusion = 1.0;
    if (HasMaterialFlag(MaterialFlags_UseOcclusionTexture))
    {
        const float textureOcclusion =
            t_Occlusion.Sample(s_MaterialSampler, input.texCoord).r;
        occlusion = lerp(1.0, textureOcclusion, g_Material.occlusionStrength);
    }

    float opacity = g_Material.opacity;
    if (HasMaterialFlag(MaterialFlags_UseOpacityTexture))
        opacity *= t_Opacity.Sample(s_MaterialSampler, input.texCoord).r;
    else if (HasMaterialFlag(MaterialFlags_UseBaseOrDiffuseTexture))
        opacity *= baseTexture.a;
    opacity = saturate(opacity);

    float3 normal = normalize(input.normal);
    float4 tangent = input.tangent;
    if (HasMaterialFlag(MaterialFlags_UseNormalTexture))
    {
        const float2 normalTexCoord =
            input.texCoord * g_Material.normalTextureTransformScale;
        normal = ApplyNormalMap(
            normal,
            tangent,
            t_Normal.Sample(s_MaterialSampler, normalTexCoord));
    }

    if (!isFrontFace)
        normal = -normal;

    const float3 baseColor = g_Material.baseOrDiffuseColor * baseTexture.rgb;
    const float roughness = saturate(g_Material.roughness * metalRoughnessTexture.g);
    const float metalnessFactor = HasMaterialFlag(MaterialFlags_MetalnessInRedChannel)
        ? metalRoughnessTexture.r
        : metalRoughnessTexture.b;
    const float metalness = saturate(g_Material.metalness * metalnessFactor);

    const float3 diffuseAlbedo =
        lerp(baseColor * (1.0 - DielectricSpecular), 0.0, metalness);
    const float3 specularF0 =
        lerp(DielectricSpecular.xxx, baseColor, metalness);

    float3 emissive = g_Material.emissiveColor;
    if (HasMaterialFlag(MaterialFlags_UseEmissiveTexture))
        emissive *= t_Emissive.Sample(s_MaterialSampler, input.texCoord).rgb;

    PSOutput output;
    output.channel0 = float4(diffuseAlbedo, opacity);
    output.channel1 = float4(specularF0, occlusion);
    output.channel2 = float4(normal, roughness);
    output.channel3 = float4(emissive, 0.0);
    return output;
}
