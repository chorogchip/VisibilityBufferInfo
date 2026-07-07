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
}

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float3 tangent : TANGENT;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float3 tangent : TANGENT;
    float3 world_pos : WORLDPOS;
    nointerpolation uint material_index : MATERIAL;
};

PSInput main(VSInput input, uint instanceID : SV_InstanceID)
{
    InstanceData instance_data = gInstance[gObjectIndex + instanceID];
    
    PSInput output;
    float4 pos_in = float4(input.position, 1.0f);
    float4 pos_world = mul(pos_in, instance_data.World);
    float4 pos_view = mul(pos_world, gView);
    float4 pos_homo = mul(pos_view, gProj);
    float3 normal_world = mul(float4(input.normal, 0.0f), instance_data.World).xyz;
    float3 tangent_world = mul(float4(input.tangent, 0.0f), instance_data.World).xyz;
    
    output.position = pos_homo;
    output.normal = mul(normal_world, (float3x3)gView);
    output.texcoord0 = input.texcoord0;
    output.texcoord1 = input.texcoord1;
    output.tangent = mul(tangent_world, (float3x3)gView);
    output.world_pos = pos_world.xyz;
    output.material_index = instance_data.material_index;
        
    return output;
}
