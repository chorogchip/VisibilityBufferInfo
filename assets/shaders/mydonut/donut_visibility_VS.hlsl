#include "..\common\mydonut_scene_abi.hlsli"

StructuredBuffer<InstanceData> t_Instances :
    register(t10, space1);
ByteAddressBuffer t_Vertices :
    register(t11, space1);
StructuredBuffer<DrawInstanceData> t_DrawInstances :
    register(t12, space1);
StructuredBuffer<uint> t_DrawInstanceIDs :
    register(t13, space1);

cbuffer c_Push : register(b1, space1)
{
    uint g_StartInstanceLocation;
    uint g_PositionOffset;
    uint g_TexCoordOffset;
};

cbuffer c_GBuffer : register(b2, space2)
{
    GBufferFillConstants g_GBuffer;
};

struct VSOutput
{
    float4 clipPosition : SV_Position;
    float2 texCoord : TEXCOORD0;
    nointerpolation uint geometryInstanceID : TEXCOORD1;
};

VSOutput main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    const uint compactedSlot = instanceID + g_StartInstanceLocation;
    const uint drawInstanceID = t_DrawInstanceIDs[compactedSlot];
    const DrawInstanceData drawInstance = t_DrawInstances[drawInstanceID];
    const InstanceData instance = t_Instances[drawInstance.instanceID];

    const float3 position = asfloat(t_Vertices.Load3(
        g_PositionOffset + vertexID * SizeOfPosition));
    const float2 texCoord = asfloat(t_Vertices.Load2(
        g_TexCoordOffset + vertexID * SizeOfTexcoord));
    const float3 worldPosition = TransformPoint(instance.transform, position);

    VSOutput output;
    output.clipPosition = mul(float4(worldPosition, 1.0), g_GBuffer.view.matWorldToClip);
    output.texCoord = texCoord;
    const uint geometryOffset =
        drawInstance.submeshID - instance.firstGeometryIndex;
    output.geometryInstanceID =
        instance.firstGeometryInstanceIndex + geometryOffset + 1;

    return output;
}
