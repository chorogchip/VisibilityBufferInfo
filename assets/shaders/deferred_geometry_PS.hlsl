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

struct MaterialData
{
    float4 base_color;
};

StructuredBuffer<MaterialData> gMaterials : register(t1);

#ifndef GBUFFER_COUNT
#define GBUFFER_COUNT 1
#endif

struct GBufferOutput
{
#if GBUFFER_COUNT >= 1
    float4 rt0 : SV_Target0;
#endif
#if GBUFFER_COUNT >= 2
    float4 rt1 : SV_Target1;
#endif
#if GBUFFER_COUNT >= 3
    float4 rt2 : SV_Target2;
#endif
#if GBUFFER_COUNT >= 4
    float4 rt3 : SV_Target3;
#endif
#if GBUFFER_COUNT >= 5
    float4 rt4 : SV_Target4;
#endif
#if GBUFFER_COUNT >= 6
    float4 rt5 : SV_Target5;
#endif
#if GBUFFER_COUNT >= 7
    float4 rt6 : SV_Target6;
#endif
#if GBUFFER_COUNT >= 8
    float4 rt7 : SV_Target7;
#endif
};


GBufferOutput main(PSInput input)
{
    GBufferOutput output;

    float3 normal = normalize(input.normal);
    float4 base_color = gMaterials[input.material_index].base_color;
    float4 gbuffer_value = float4(base_color.rgb * (normal.z * 0.5f + 0.5f), base_color.a);

    
#if GBUFFER_COUNT >= 1
    output.rt0 = gbuffer_value;
 #endif
#if GBUFFER_COUNT >= 2
    output.rt1 = gbuffer_value;
#endif
#if GBUFFER_COUNT >= 3
    output.rt2 = gbuffer_value;
#endif
#if GBUFFER_COUNT >= 4
    output.rt3 = gbuffer_value;
#endif
#if GBUFFER_COUNT >= 5
    output.rt4 = gbuffer_value;
#endif
#if GBUFFER_COUNT >= 6
    output.rt5 = gbuffer_value;
#endif
#if GBUFFER_COUNT >= 7
    output.rt6 = gbuffer_value;
#endif
#if GBUFFER_COUNT >= 8
    output.rt7 = gbuffer_value;
#endif
    return output;
}
