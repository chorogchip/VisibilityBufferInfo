#include "common_input_struct.hlsli"

struct DebugPSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float3 tangent : TANGENT;
    float3 world_pos : WORLDPOS;

    nointerpolation uint material_index : MATERIAL;
    nointerpolation uint instance_id : INSTANCE;
    nointerpolation uint object_id : OBJECT;
    nointerpolation uint mesh_index : MESH;
    nointerpolation uint flags : FLAGS;
    nointerpolation uint vertex_id : VERTEXID;
};

cbuffer MatricesCB : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
};

struct InstanceData
{
    uint object_id;
    uint material_index;
    uint mesh_index;
    uint flags;
    float4x4 World;
};

StructuredBuffer<InstanceData> gInstance : register(t0);

cbuffer DrawCB : register(b1)
{
    uint gObjectIndex;
};

DebugPSInput main(
    VSInput input,
    uint instanceID : SV_InstanceID,
    uint vertexID : SV_VertexID
)
{
    const uint instanceIndex = gObjectIndex + instanceID;
    const InstanceData instanceData = gInstance[instanceIndex];

    const float4 positionLocal = float4(input.position, 1.0f);
    const float4 positionWorld = mul(positionLocal, instanceData.World);
    const float4 positionView = mul(positionWorld, gView);

    const float3 normalWorld = normalize(
        mul(input.normal, (float3x3) instanceData.World)
    );

    float3 tangentWorld = normalize(
        mul(input.tangent, (float3x3) instanceData.World)
    );

    tangentWorld = normalize(
        tangentWorld - normalWorld * dot(tangentWorld, normalWorld)
    );

    DebugPSInput output;
    output.position = mul(positionView, gProj);
    output.normal = normalize(mul(normalWorld, (float3x3) gView));
    output.texcoord0 = input.texcoord0;
    output.texcoord1 = input.texcoord1;
    output.tangent = normalize(mul(tangentWorld, (float3x3) gView));
    output.world_pos = positionWorld.xyz;
    output.material_index = instanceData.material_index;
    output.instance_id = instanceIndex;
    output.object_id = instanceData.object_id;
    output.mesh_index = instanceData.mesh_index;
    output.flags = instanceData.flags;
    output.vertex_id = vertexID;

    return output;
}