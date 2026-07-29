#include "..\common\jungle_point_scene_abi.hlsli"

ByteAddressBuffer t_Vertices :
    register(t11, space1);
StructuredBuffer<JunglePointInstanceData> t_PointInstances :
    register(t14, space1);
StructuredBuffer<JunglePointPrototypeData> t_PointPrototypes :
    register(t15, space1);
StructuredBuffer<uint> t_PointInstanceIDs :
    register(t16, space1);

struct JungleDepthPushConstants
{
    uint startInstanceLocation;
    uint prototypeID;
    uint positionOffset;
    uint texCoordOffset;
};

cbuffer c_Push : register(b1, space1)
{
    JungleDepthPushConstants g_Push;
};

cbuffer c_Depth : register(b2, space2)
{
    DepthPassConstants g_Depth;
};

void main(
    uint vertexId : SV_VertexID,
    uint instanceId : SV_InstanceID,
    out float4 position : SV_Position,
    out float2 texCoord : TEXCOORD)
{
    const uint pointInstanceID =
        t_PointInstanceIDs[
            instanceId + g_Push.startInstanceLocation];
    const JunglePointInstanceData pointInstance =
        t_PointInstances[pointInstanceID];
    const JunglePointPrototypeData prototype =
        t_PointPrototypes[g_Push.prototypeID];

    const float3 localPosition = asfloat(t_Vertices.Load3(
        g_Push.positionOffset + vertexId * SizeOfPosition));
    texCoord = asfloat(t_Vertices.Load2(
        g_Push.texCoordOffset + vertexId * SizeOfTexcoord));
    const float3 worldPosition = JungleTransformPoint(
        pointInstance,
        prototype,
        localPosition);
    position = mul(
        float4(worldPosition, 1.0),
        g_Depth.matWorldToClip);
}
