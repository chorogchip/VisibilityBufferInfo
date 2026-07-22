#ifndef DONUT_COMMON_INPUT_HLSLI
#define DONUT_COMMON_INPUT_HLSLI

struct VSInput
{
    uint vertex_id : SV_VertexID;
    uint instance_id : SV_InstanceID;
};

struct PSInput
{
    float4 pos : SV_Position;
    float3 pos_world : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
    float3 normal : TEXCOORD2;
    float4 tangent : TEXCOORD3;
};

struct PushConstants
{
    uint start_instance_location;
    uint start_vertex_location;
};

struct ViewConstant
{
    uint position_offset;
    uint texcoord_offset;
    uint normal_offset;
    uint tangent_offset;
};

struct MaterialConstant
{
    
};

// scene, textures, sampler

#endif
