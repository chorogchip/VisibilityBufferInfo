cbuffer MatricesCB : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
};

cbuffer DrawCB : register(b1)
{
    uint gInstanceIndex;
};


struct InstanceData
{
    float4x4 World;
};

StructuredBuffer<InstanceData> gInstance : register(t0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord: TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

PSInput main(VSInput input, uint instanceID : SV_InstanceID)
{
    InstanceData instance_data = gInstance[gInstanceIndex];
    
    
    PSInput output;
    float4 pos_in = float4(input.position, 1.0f);
    float4 pos_world = mul(pos_in, instance_data.World);
    float4 pos_view = mul(pos_world, gView);
    float4 pos_homo = mul(pos_view, gProj);
    
    output.position = pos_homo;
    output.color = float4(input.texcoord, 0.0f, 1.0f);
        
    return output;
}