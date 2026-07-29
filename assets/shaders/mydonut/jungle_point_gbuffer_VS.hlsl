#include "..\common\jungle_point_scene_abi.hlsli"

ByteAddressBuffer t_Vertices :
    register(t11, space1);
StructuredBuffer<JunglePointInstanceData> t_PointInstances :
    register(t14, space1);
StructuredBuffer<JunglePointPrototypeData> t_PointPrototypes :
    register(t15, space1);
StructuredBuffer<uint> t_PointInstanceIDs :
    register(t16, space1);

struct JungleGBufferPushConstants
{
    uint startInstanceLocation;
    uint prototypeID;
    uint positionOffset;
    uint prevPositionOffset;
    uint texCoordOffset;
    uint normalOffset;
    uint tangentOffset;
    uint pad0;
};

cbuffer c_Push : register(b1, space1)
{
    JungleGBufferPushConstants g_Push;
};

cbuffer c_GBuffer : register(b2, space2)
{
    GBufferFillConstants g_GBuffer;
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
    const uint pointInstanceID =
        t_PointInstanceIDs[
            instanceId + g_Push.startInstanceLocation];
    const JunglePointInstanceData pointInstance =
        t_PointInstances[pointInstanceID];
    const JunglePointPrototypeData prototype =
        t_PointPrototypes[g_Push.prototypeID];

    const float3 position = asfloat(t_Vertices.Load3(
        g_Push.positionOffset + vertexId * SizeOfPosition));
    const float2 texCoord = asfloat(t_Vertices.Load2(
        g_Push.texCoordOffset + vertexId * SizeOfTexcoord));
    const uint packedNormal = t_Vertices.Load(
        g_Push.normalOffset + vertexId * SizeOfPackedNormal);
    const uint packedTangent = t_Vertices.Load(
        g_Push.tangentOffset + vertexId * SizeOfPackedNormal);

    const float3 worldPosition = JungleTransformPoint(
        pointInstance,
        prototype,
        position);
    const float3 worldNormal = JungleTransformVector(
        pointInstance,
        prototype,
        UnpackRgb8Snorm(packedNormal));
    const float4 tangent = UnpackRgba8Snorm(packedTangent);
    const float3 worldTangent = JungleTransformVector(
        pointInstance,
        prototype,
        tangent.xyz);

    VSOutput output;
    output.clipPosition = mul(
        float4(worldPosition, 1.0),
        g_GBuffer.view.matWorldToClip);
    output.texCoord = texCoord;
    output.normal = worldNormal;
    output.tangent = float4(worldTangent, tangent.w);
    return output;
}
