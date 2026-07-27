#include "common_input_struct.hlsli"

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

struct PSInputVisbuf
{
    float4 position : SV_POSITION;
    nointerpolation uint object_id : OBJECTID;
};

PSInputVisbuf main(VSInput input, uint instanceID : SV_InstanceID)
{
    const uint drawInstanceID = gDrawInstanceIDs[gStartInstance + instanceID];
    const DrawInstanceData drawInstance = gDrawInstances[drawInstanceID];
    const InstanceData instance_data = gInstances[drawInstance.instance_id];
    
    PSInputVisbuf output;
    float4 pos_in = float4(input.position, 1.0f);
    float4 pos_world = mul(pos_in, instance_data.World);
    float4 pos_view = mul(pos_world, gView);
    float4 pos_homo = mul(pos_view, gProj);
    
    output.position = pos_homo;
    output.object_id = drawInstanceID + 1;
    return output;
}
