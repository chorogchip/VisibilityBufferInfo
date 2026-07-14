#ifndef COMMON_GBUFFER_HLSLI
#define COMMON_GBUFFER_HLSLI


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

GBufferOutput make_gbuffer_output(float4 value)
{
    GBufferOutput output;
    
#if GBUFFER_COUNT >= 1
    output.rt0 = value;
#endif
#if GBUFFER_COUNT >= 2
    output.rt1 = value;
#endif
#if GBUFFER_COUNT >= 3
    output.rt2 = value;
#endif
#if GBUFFER_COUNT >= 4
    output.rt3 = value;
#endif
#if GBUFFER_COUNT >= 5
    output.rt4 = value;
#endif
#if GBUFFER_COUNT >= 6
    output.rt5 = value;
#endif
#if GBUFFER_COUNT >= 7
    output.rt6 = value;
#endif
#if GBUFFER_COUNT >= 8
    output.rt7 = value;
#endif
    return output;
}


#endif  // COMMON_GBUFFER_HLSLI