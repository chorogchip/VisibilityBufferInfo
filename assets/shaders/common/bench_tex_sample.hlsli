#ifndef COMMON_BENCH_TEX_SAMPLE_HLSLI
#define COMMON_BENCH_TEX_SAMPLE_HLSLI

#ifndef ALU_CALC_COUNT
#define ALU_CALC_COUNT 1
#endif

#define ALU_LANES 8

Texture2D<float4> gTextures[512] : register(t8);
SamplerState gSampler : register(s0);

float3 apply_alu_workload(float3 normal)
{
    float3 lanes[ALU_LANES];
    
    [unroll]
    for (uint lane = 0; lane < ALU_LANES; ++lane)
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
        for (uint lane = 0; lane < ALU_LANES; ++lane)
        {
            float lane_f = float(lane + 1);
            
            float3 vec_add = float3(0.071f, 0.113f, 0.157f) * lane_f;
            float3 vec_mul = float3(0.073f, 0.119f, 0.146f) * lane_f;
        
            lanes[lane] = frac(lanes[lane] * vec_mul + vec_add);
        }
    }
    
    float3 result = float3(0.0f, 0.0f, 0.0f);
    
    [unroll]
    for (uint lane2 = 0; lane2 < ALU_LANES; ++lane2)
    {
        result += lanes[lane2];
    }
    
    return result * rcp(float(ALU_LANES));
}

float3 apply_workload(
    uint index,
    float2 uv,
    float2 ddx, float2 ddy,
    float3 normal
)
{
    return
        gTextures[index].SampleGrad(gSampler, uv, ddx, ddy).rgb +
        apply_alu_workload(normal) * 0.001f;

}

float3 apply_workload_nonuniform(
    uint index,
    float2 uv,
    float2 ddx, float2 ddy,
    float3 normal
)
{
    return
        gTextures[NonUniformResourceIndex(index)].SampleGrad(gSampler, uv, ddx, ddy).rgb +
        apply_alu_workload(normal) * 0.001f;
}

#endif  // COMMON_BENCH_TEX_SAMPLE_HLSLI