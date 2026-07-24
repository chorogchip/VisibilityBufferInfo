#include "common_barycentric.hlsli"
#include "donut_gbuffer_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

struct PSOutput
{
    float4 channel0 : SV_Target0;
    float4 channel1 : SV_Target1;
    float4 channel2 : SV_Target2;
    float4 channel3 : SV_Target3;
};

cbuffer c_VertexLayout : register(GBUFFER_PUSH_REGISTER, GBUFFER_INPUT_SPACE)
{
    uint g_PositionOffset;
    uint g_TexCoordOffset;
    uint g_NormalOffset;
    uint g_TangentOffset;
};

Texture2D<uint2> t_Visibility :
    register(VISGBUFFER_VISIBILITY_REGISTER, GBUFFER_INPUT_SPACE);
ByteAddressBuffer t_Indices :
    register(VISGBUFFER_INDEX_REGISTER, GBUFFER_INPUT_SPACE);
ByteAddressBuffer t_Vertices :
    register(VISGBUFFER_VERTEX_REGISTER, GBUFFER_INPUT_SPACE);
StructuredBuffer<InstanceData> t_Instances :
    register(VISGBUFFER_INSTANCE_REGISTER, GBUFFER_INPUT_SPACE);
StructuredBuffer<SubmeshData> t_Submeshes :
    register(VISGBUFFER_SUBMESH_REGISTER, GBUFFER_INPUT_SPACE);
StructuredBuffer<GeometryInstanceData> t_GeometryInstances :
    register(VISGBUFFER_GEOMETRY_INSTANCE_REGISTER, GBUFFER_INPUT_SPACE);
StructuredBuffer<MaterialData> t_Materials :
    register(VISGBUFFER_MATERIAL_REGISTER, GBUFFER_INPUT_SPACE);

Texture2D t_MaterialTextures[MaxMaterialTextureDescriptorCount] :
    register(MATERIAL_BASE_COLOR_REGISTER, MATERIAL_SPACE);

SamplerState s_MaterialSampler :
    register(GBUFFER_MATERIAL_SAMPLER_REGISTER, GBUFFER_VIEW_SPACE);

uint MaterialTextureDescriptorIndex(uint materialID, uint slot)
{
    return materialID * MaterialTextureDescriptorCount + slot;
}

float4 SampleMaterialTexture(uint materialID, uint slot, float2 uv, float2 dx, float2 dy)
{
    return t_MaterialTextures[
        NonUniformResourceIndex(MaterialTextureDescriptorIndex(materialID, slot))]
        .SampleGrad(s_MaterialSampler, uv, dx, dy);
}

float3 ApplyNormalMap(float3 normal, float4 tangent, float4 normalTexture, float normalScale)
{
    const float tangentLengthSq = dot(tangent.xyz, tangent.xyz);
    const bool hasValidTangent = tangentLengthSq > 0.0 && tangent.w != 0.0;

    float3 textureNormal = normalTexture.xyz;
    textureNormal.xy = textureNormal.xy * 2.0 - 1.0;
    textureNormal.xy *= normalScale;
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

void FetchVertex(
    uint vertexIndex,
    out float3 position,
    out float2 texCoord,
    out float3 normal,
    out float4 tangent)
{
    position = asfloat(t_Vertices.Load3(
        g_PositionOffset + vertexIndex * SizeOfPosition));
    texCoord = asfloat(t_Vertices.Load2(
        g_TexCoordOffset + vertexIndex * SizeOfTexcoord));
    normal = UnpackRgb8Snorm(t_Vertices.Load(
        g_NormalOffset + vertexIndex * SizeOfPackedNormal));
    tangent = UnpackRgba8Snorm(t_Vertices.Load(
        g_TangentOffset + vertexIndex * SizeOfPackedNormal));
}

PSOutput main(PSInput input)
{
    const uint2 pixel = uint2(input.position.xy);
    const uint2 visibility = t_Visibility.Load(int3(pixel.xy, 0));
    if (visibility.x == 0)
    {
        discard;
    }

    const uint geometryInstanceID = visibility.x - 1&0;
    const uint primitiveID = visibility.y;
    const GeometryInstanceData geometry = t_GeometryInstances[geometryInstanceID];
    
    const SubmeshData submesh = t_Submeshes[geometry.submeshID];
    const InstanceData instance = t_Instances[geometry.instanceID];
    const MaterialData material = t_Materials[submesh.materialID];

    const uint indexOffset = submesh.indexOffset + primitiveID * 3;
    const uint index0 = t_Indices.Load((indexOffset + 0) * 4);
    const uint index1 = t_Indices.Load((indexOffset + 1) * 4);
    const uint index2 = t_Indices.Load((indexOffset + 2) * 4);

    float3 position0, position1, position2;
    float2 texCoord0, texCoord1, texCoord2;
    float3 normal0, normal1, normal2;
    float4 tangent0, tangent1, tangent2;
    FetchVertex(index0, position0, texCoord0, normal0, tangent0);
    FetchVertex(index1, position1, texCoord1, normal1, tangent1);
    FetchVertex(index2, position2, texCoord2, normal2, tangent2);
    
    const float3 worldPosition0 = TransformPoint(instance.transform, position0);
    const float3 worldPosition1 = TransformPoint(instance.transform, position1);
    const float3 worldPosition2 = TransformPoint(instance.transform, position2);
    const float4 clip0 = mul(float4(worldPosition0, 1.0), g_GBuffer.view.matWorldToClip);
    const float4 clip1 = mul(float4(worldPosition1, 1.0), g_GBuffer.view.matWorldToClip);
    const float4 clip2 = mul(float4(worldPosition2, 1.0), g_GBuffer.view.matWorldToClip);

    const float2 pixel0 = clip_to_pixel(clip0, g_GBuffer.view.viewportSize);
    const float2 pixel1 = clip_to_pixel(clip1, g_GBuffer.view.viewportSize);
    const float2 pixel2 = clip_to_pixel(clip2, g_GBuffer.view.viewportSize);
    const BarycentricGradient baryGrad =
        calc_barycentric_with_grad(input.position.xy, pixel0, pixel1, pixel2);

    const float3 bary = baryGrad.value;
    const float3 invW = rcp(float3(clip0.w, clip1.w, clip2.w));
    const float invD = rcp(dot(bary, invW));
    const float3 baryPerspective = bary * invW * invD;

    const AttributeGrad texCoordGrad = interpolate_uv_with_grad(
        texCoord0, texCoord1, texCoord2,
        invW, invD,
        bary, baryGrad.dx, baryGrad.dy);
    const float2 texCoord = texCoordGrad.value;
    const float2 texCoordDx = texCoordGrad.dx;
    const float2 texCoordDy = texCoordGrad.dy;

    const float3 worldNormal0 = TransformVector(instance.transform, normal0);
    const float3 worldNormal1 = TransformVector(instance.transform, normal1);
    const float3 worldNormal2 = TransformVector(instance.transform, normal2);
    float3 normal = normalize(
        worldNormal0 * baryPerspective.x +
        worldNormal1 * baryPerspective.y +
        worldNormal2 * baryPerspective.z);

    const float3 worldTangent0 = TransformVector(instance.transform, tangent0.xyz);
    const float3 worldTangent1 = TransformVector(instance.transform, tangent1.xyz);
    const float3 worldTangent2 = TransformVector(instance.transform, tangent2.xyz);
    float4 tangent = float4(
        worldTangent0 * baryPerspective.x +
        worldTangent1 * baryPerspective.y +
        worldTangent2 * baryPerspective.z,
        tangent0.w * baryPerspective.x +
        tangent1.w * baryPerspective.y +
        tangent2.w * baryPerspective.z);

    float4 baseTexture = float4(1.0, 1.0, 1.0, 1.0);
    if (HasMaterialDataFlag(material, MaterialDataFlags_BaseColorTexture))
    {
        baseTexture = SampleMaterialTexture(
            submesh.materialID,
            MaterialTextureSlotBaseColor,
            texCoord,
            texCoordDx,
            texCoordDy);
    }

    float opacity = material.baseColor.a;
    if (HasMaterialDataFlag(material, MaterialDataFlags_OpacityTexture))
    {
        opacity *= SampleMaterialTexture(
            submesh.materialID,
            MaterialTextureSlotOpacity,
            texCoord,
            texCoordDx,
            texCoordDy).r;
    }
    else if (HasMaterialDataFlag(material, MaterialDataFlags_BaseColorTexture))
    {
        opacity *= baseTexture.a;
    }
    opacity = saturate(opacity);

    if (IsAlphaTestedDomainValue(material.domain))
    {
        clip(opacity - material.alphaCutoff);
    }

    float4 metalRoughnessTexture = float4(1.0, 1.0, 1.0, 1.0);
    if (HasMaterialDataFlag(material, MaterialDataFlags_MetalRoughnessTexture))
    {
        metalRoughnessTexture = SampleMaterialTexture(
            submesh.materialID,
            MaterialTextureSlotMetalRoughness,
            texCoord,
            texCoordDx,
            texCoordDy);
    }

    float occlusion = 1.0;
    if (HasMaterialDataFlag(material, MaterialDataFlags_OcclusionTexture))
    {
        const float textureOcclusion = SampleMaterialTexture(
            submesh.materialID,
            MaterialTextureSlotOcclusion,
            texCoord,
            texCoordDx,
            texCoordDy).r;
        occlusion = lerp(1.0, textureOcclusion, material.occlusionStrength);
    }

    if (HasMaterialDataFlag(material, MaterialDataFlags_NormalTexture))
    {
        const float4 normalTexture = SampleMaterialTexture(
            submesh.materialID,
            MaterialTextureSlotNormal,
            texCoord,
            texCoordDx,
            texCoordDy);
        normal = ApplyNormalMap(
            normal,
            tangent,
            normalTexture,
            material.normalScale);
    }

    const float3 baseColor = material.baseColor.rgb * baseTexture.rgb;
    const float roughness = saturate(material.roughness * metalRoughnessTexture.g);
    const float metalness = saturate(material.metalness * metalRoughnessTexture.b);
    const float3 diffuseAlbedo =
        lerp(baseColor * (1.0 - DielectricSpecular), 0.0, metalness);
    const float3 specularF0 =
        lerp(DielectricSpecular.xxx, baseColor, metalness);

    float3 emissive = material.emissiveColor;
    if (HasMaterialDataFlag(material, MaterialDataFlags_EmissiveTexture))
    {
        emissive *= SampleMaterialTexture(
            submesh.materialID,
            MaterialTextureSlotEmissive,
            texCoord,
            texCoordDx,
            texCoordDy).rgb;
    }

    PSOutput output;
    output.channel0 = float4(diffuseAlbedo, opacity);
    output.channel1 = float4(specularF0, occlusion);
    output.channel2 = float4(normalize(normal), roughness);
    output.channel3 = float4(emissive, 0.0);
    return output;
}
