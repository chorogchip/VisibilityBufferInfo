#include "..\common\common_input_struct.hlsli"
    
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
}

PSInput main(VSInput input, uint instanceID : SV_InstanceID)
{
    const uint drawInstanceID = gDrawInstanceIDs[gStartInstance + instanceID];
    const DrawInstanceData drawInstance = gDrawInstances[drawInstanceID];
    const InstanceData instance_data = gInstances[drawInstance.instance_id];
    
    PSInput output;
    
    float4 pos_in = float4(input.position, 1.0f);
    float4 pos_world = mul(pos_in, instance_data.World);
    float4 pos_view = mul(pos_world, gView);
    float4 pos_homo = mul(pos_view, gProj);
    float3 normal_world = mul(float4(input.normal, 0.0f), instance_data.World).xyz;
    float3 tangent_world = mul(
        float4(input.tangent.xyz, 0.0f),
        instance_data.World).xyz;
    
    output.position = pos_homo;
    output.normal = mul(normal_world, (float3x3)gView);
    output.texcoord0 = input.texcoord0;
    output.texcoord1 = input.texcoord0;
    output.tangent = mul(tangent_world, (float3x3)gView);
    output.material_index = gMaterialID;
    
    return output;
}
