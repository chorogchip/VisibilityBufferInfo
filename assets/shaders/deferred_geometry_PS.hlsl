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

#ifndef TEXTURE_COUNT
#define TEXTURE_COUNT 1
#endif

#ifndef TEXTURE_SAMPLING_COUNT
#define TEXTURE_SAMPLING_COUNT 1
#endif

#ifndef TEXTURE_SIZE
#define TEXTURE_SIZE 1
#endif

#ifndef ALU_CALC_COUNT
#define ALU_CALC_COUNT 1
#endif

#if TEXTURE_SIZE < 1
#define WORKLOAD_TEXTURE_SIZE 1
#else
#define WORKLOAD_TEXTURE_SIZE TEXTURE_SIZE
#endif

#if TEXTURE_COUNT > 0
Texture2D<float4> gTextures[TEXTURE_COUNT] : register(t8);
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

float3 apply_workload(float3 color, float2 pixel)
{
    float3 ret = color;

#if TEXTURE_COUNT > 0 && TEXTURE_SAMPLING_COUNT > 0
    uint2 base_texel = uint2(pixel) % uint2(WORKLOAD_TEXTURE_SIZE, WORKLOAD_TEXTURE_SIZE);
    [loop]
    for (uint i = 0; i < TEXTURE_SAMPLING_COUNT; ++i) {
        uint texture_index = i % TEXTURE_COUNT;
        uint2 texel = (base_texel + uint2(i * 17, i * 31)) % uint2(WORKLOAD_TEXTURE_SIZE, WORKLOAD_TEXTURE_SIZE);
        ret += gTextures[texture_index].Load(int3(texel, 0)).rgb * 0.001f;
    }
#endif

#if ALU_CALC_COUNT > 0
    float3 v = ret;
    [loop]
    for (uint i = 0; i < ALU_CALC_COUNT; ++i) {
        v = v * 1.00013f + float3(0.00031f, 0.00071f, 0.00111f);
    }
    ret += v * 0.000001f;
#endif

    return ret;
}

GBufferOutput main(PSInput input)
{
    GBufferOutput output;

    float3 normal = normalize(input.normal);
    float4 base_color = gMaterials[input.material_index].base_color;
    float3 color = base_color.rgb * (normal.z * 0.5f + 0.5f);
    float4 gbuffer_value = float4(apply_workload(color, input.position.xy), base_color.a);

    
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
