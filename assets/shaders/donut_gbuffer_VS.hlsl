#include "donut_gbuffer_common.hlsli"

StructuredBuffer<InstanceData> t_Instances :
    register(GBUFFER_INSTANCE_REGISTER, GBUFFER_INPUT_SPACE);
ByteAddressBuffer t_Vertices :
    register(GBUFFER_VERTEX_REGISTER, GBUFFER_INPUT_SPACE);

cbuffer c_Push : register(GBUFFER_PUSH_REGISTER, GBUFFER_INPUT_SPACE)
{
    GBufferPushConstants g_Push;
};

struct VSOutput
{
    float4 clipPosition : SV_Position;
    float2 texCoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 tangent : TANGENT0;
};

VSOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    const uint instanceIndex = instanceId + g_Push.startInstanceLocation;
    const uint vertexIndex = vertexId + g_Push.startVertexLocation;
    const InstanceData instance = t_Instances[instanceIndex];

    const float3 position = asfloat(t_Vertices.Load3(
        g_Push.positionOffset + vertexIndex * SizeOfPosition));
    const float2 texCoord = asfloat(t_Vertices.Load2(
        g_Push.texCoordOffset + vertexIndex * SizeOfTexcoord));
    const uint packedNormal = t_Vertices.Load(
        g_Push.normalOffset + vertexIndex * SizeOfPackedNormal);
    const uint packedTangent = t_Vertices.Load(
        g_Push.tangentOffset + vertexIndex * SizeOfPackedNormal);

    const float3 worldPosition = TransformPoint(instance.transform, position);
    const float3 worldNormal = TransformVector(
        instance.transform, UnpackRgb8Snorm(packedNormal));
    const float4 tangent = UnpackRgba8Snorm(packedTangent);
    const float3 worldTangent = TransformVector(instance.transform, tangent.xyz);

    VSOutput output;
    output.clipPosition = mul(float4(worldPosition, 1.0), g_GBuffer.view.matWorldToClip);
    output.texCoord = texCoord;
    output.normal = worldNormal;
    output.tangent = float4(worldTangent, tangent.w);
    return output;
}
