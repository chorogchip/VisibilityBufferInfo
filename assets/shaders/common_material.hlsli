#ifndef COMMON_MATERIAL_HLSLI
#define COMMON_MATERIAL_HLSLI

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

#if TEXTURE_COUNT > 0
Texture2D<float4> gTextures[TEXTURE_COUNT] : register(t8);
SamplerState gSampler[1] : register(s0);
#endif

float3 apply_alu_workload(float3 normal)
{
#define LANES 8
    float3 lanes[LANES];
    
    [unroll]
    for (uint lane = 0; lane < LANES; ++lane)
    {
        float lane_f = float(lane + 1);
        
        float3 vec_add = float3(0.071f, 0.113f, 0.157f) * lane_f;
        float3 vec_mul = float3(0.073f, 0.119f, 0.146f) * lane_f;
        
        lanes[lane] = frac(normal * vec_mul + vec_add);
    }
    
    [loop]
    for (uint i = 0; i < ALU_CALC_COUNT; ++i)
    {
        [unroll]
        for (uint lane = 0; lane < LANES; ++lane)
        {
            float lane_f = float(lane + 1);
            
            float3 vec_add = float3(0.071f, 0.113f, 0.157f) * lane_f;
            float3 vec_mul = float3(0.073f, 0.119f, 0.146f) * lane_f;
        
            lanes[lane] = frac(lanes[lane] * vec_mul + vec_add);
        }
    }
    
    float3 result = float3(0.0f, 0.0f, 0.0f);
    
    [unroll]
    for (uint lane2 = 0; lane2 < LANES; ++lane2)
    {
        result += lanes[lane2];
    }
    
    return result * rcp(float(LANES));
    
#undef LANES
}

float3 apply_workload(
    float2 uv,
    float2 ddx, float2 ddy,
    float3 normal
)
{
    float3 result_tex = float3(1.0f, 1.0f, 1.0f);
    
#if TEXTURE_COUNT > 0 && TEXTURE_SAMPLING_COUNT > 0
    result_tex = float3(0.0f, 0.0f, 0.0f);
    
    [unroll]
    for (uint it = 0; it < TEXTURE_COUNT; ++it)
    {
        [loop]
        for (uint i = 0; i < TEXTURE_SAMPLING_COUNT; ++i)
        {
            float texel_size = 1.0f / float(TEXTURE_SIZE);
            float2 sample_uv = uv + float2(i, i) * texel_size;
            
            result_tex += gTextures[it].SampleGrad(
                gSampler[0],
                sample_uv,
                ddx, ddy).rgb;
        }
    }
    
    result_tex *= rcp(float(TEXTURE_COUNT * TEXTURE_SAMPLING_COUNT));
#endif
    
    float3 result_alu = float3(1.0f, 1.0f, 1.0f);
   
#if ALU_CALC_COUNT > 0
    result_alu = apply_alu_workload(normal);
#endif
    
    return result_tex + result_alu * 0.001f;

}

#endif  // COMMON_MATERIAL_HLSLI