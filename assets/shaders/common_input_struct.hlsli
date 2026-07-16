#ifndef COMMON_INPUT_STRUCT_HLSLI
#define COMMON_INPUT_STRUCT_HLSLI

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

#endif  // COMMON_IO_STRUCT_HLSLI