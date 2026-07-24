#include "donut_gbuffer_common.hlsli"

StructuredBuffer<InstanceData> t_Instances :
    register(GBUFFER_INSTANCE_REGISTER, GBUFFER_INPUT_SPACE);
ByteAddressBuffer t_Vertices :
    register(GBUFFER_VERTEX_REGISTER, GBUFFER_INPUT_SPACE);

cbuffer c_Push : register(GBUFFER_PUSH_REGISTER, GBUFFER_INPUT_SPACE)
{
    uint g_StartInstanceLocation;
    uint g_SubmeshID;
    uint g_PositionOffset;
    uint g_TexCoordOffset;
};

struct VSOutput
{
    float4 clipPosition : SV_Position;
    float2 texCoord : TEXCOORD0;
    nointerpolation uint geometryInstanceID : TEXCOORD1;
};

VSOutput main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    const uint instanceIndex = instanceID + g_StartInstanceLocation;
    const InstanceData instance = t_Instances[instanceIndex];
    const uint localGeometryID = g_SubmeshID - instance.firstGeometryIndex;

    const float3 position = asfloat(t_Vertices.Load3(
        g_PositionOffset + vertexID * SizeOfPosition));
    const float2 texCoord = asfloat(t_Vertices.Load2(
        g_TexCoordOffset + vertexID * SizeOfTexcoord));
    const float3 worldPosition = TransformPoint(instance.transform, position);

    VSOutput output;
    output.clipPosition = mul(float4(worldPosition, 1.0), g_GBuffer.view.matWorldToClip);
    output.texCoord = texCoord;
    output.geometryInstanceID =
        instance.firstGeometryInstanceIndex + localGeometryID + 1;
    
    return output;
}