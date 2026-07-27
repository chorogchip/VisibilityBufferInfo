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
    uint instance_id;
    uint pad0;
    uint pad1;
    uint pad2;
    float4x4 World;
};

struct DrawInstanceData
{
    uint instance_id;
    uint submesh_id;
};

StructuredBuffer<InstanceData> gInstances : register(t0);
StructuredBuffer<DrawInstanceData> gDrawInstances : register(t10);
StructuredBuffer<uint> gDrawInstanceIDs : register(t11);

cbuffer DrawCB : register(b1)
{
    uint gStartInstance;
    uint gMaterialID;
};

DebugPSInput main(
    VSInput input,
    uint instanceID : SV_InstanceID,
    uint vertexID : SV_VertexID
)
{
    const uint drawInstanceID = gDrawInstanceIDs[gStartInstance + instanceID];
    const DrawInstanceData drawInstance = gDrawInstances[drawInstanceID];
    const InstanceData instanceData = gInstances[drawInstance.instance_id];

    const float4 positionLocal = float4(input.position, 1.0f);
    const float4 positionWorld = mul(positionLocal, instanceData.World);
    const float4 positionView = mul(positionWorld, gView);

    const float3 normalWorld = normalize(
        mul(input.normal, (float3x3) instanceData.World)
    );

    float3 tangentWorld = normalize(
        mul(input.tangent.xyz, (float3x3) instanceData.World)
    );

    tangentWorld = normalize(
        tangentWorld - normalWorld * dot(tangentWorld, normalWorld)
    );

    DebugPSInput output;
    output.position = mul(positionView, gProj);
    output.normal = normalize(mul(normalWorld, (float3x3) gView));
    output.texcoord0 = input.texcoord0;
    output.texcoord1 = input.texcoord0;
    output.tangent = normalize(mul(tangentWorld, (float3x3) gView));
    output.world_pos = positionWorld.xyz;
    output.material_index = gMaterialID;
    output.instance_id = instanceData.instance_id;
    output.object_id = drawInstanceID;
    output.mesh_index = drawInstance.submesh_id;
    output.flags = 0;
    output.vertex_id = vertexID;

    return output;
}
