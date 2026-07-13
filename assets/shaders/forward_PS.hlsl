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

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float3 tangent : TANGENT;
    nointerpolation uint material_index : MATERIAL;
};

struct MaterialData
{
    float4 base_color;
};

StructuredBuffer<MaterialData> gMaterials : register(t1);

#if TEXTURE_COUNT > 0
Texture2D<float4> gTextures[TEXTURE_COUNT] : register(t8);
SamplerState gSamplers[TEXTURE_COUNT] : register(s0);
#endif

float4 SampleTexture(uint textureIndex, float2 uv)
{
    uint index = NonUniformResourceIndex(textureIndex);
    return gTextures[index].Sample(gSamplers[index], uv);
}

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

float4 main(PSInput input, uint triangleID : SV_PrimitiveID) : SV_TARGET
{
    return float4(frac(float(triangleID + input.material_index) * 0.2345f), frac(float(triangleID + input.material_index) * 0.56789f), frac(float(triangleID + input.material_index) * 0.1234f), 1.0f);
    float3 normal = normalize(input.normal);
    float4 base_color = gMaterials[input.material_index].base_color;
    float3 color = base_color.rgb * (normal.z * 0.5f + 0.5f);
    return float4(apply_workload(color, input.position.xy), base_color.a);
}
