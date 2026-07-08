#ifndef GBUFFER_COUNT
#define GBUFFER_COUNT 1
#endif

#if GBUFFER_COUNT >= 1
Texture2D gbuffer0 : register(t0);
#endif
#if GBUFFER_COUNT >= 2
Texture2D gbuffer1 : register(t1);
#endif
#if GBUFFER_COUNT >= 3
Texture2D gbuffer2 : register(t2);
#endif
#if GBUFFER_COUNT >= 4
Texture2D gbuffer3 : register(t3);
#endif
#if GBUFFER_COUNT >= 5
Texture2D gbuffer4 : register(t4);
#endif
#if GBUFFER_COUNT >= 6
Texture2D gbuffer5 : register(t5);
#endif
#if GBUFFER_COUNT >= 7
Texture2D gbuffer6 : register(t6);
#endif
#if GBUFFER_COUNT >= 8
Texture2D gbuffer7 : register(t7);
#endif

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(PSInput input) : SV_Target
{
    int3 coord = int3(input.position.xy, 0);
    
#if GBUFFER_COUNT >= 1
    float4 rt0 = gbuffer0.Load(coord);
#endif
#if GBUFFER_COUNT >= 2
    float4 rt1 = gbuffer1.Load(coord);
#endif
#if GBUFFER_COUNT >= 3
    float4 rt2 = gbuffer2.Load(coord);
#endif
#if GBUFFER_COUNT >= 4
    float4 rt3 = gbuffer3.Load(coord);
#endif
#if GBUFFER_COUNT >= 5
    float4 rt4 = gbuffer4.Load(coord);
#endif
#if GBUFFER_COUNT >= 6
    float4 rt5 = gbuffer5.Load(coord);
#endif
#if GBUFFER_COUNT >= 7
    float4 rt6 = gbuffer6.Load(coord);
#endif
#if GBUFFER_COUNT >= 8
    float4 rt7 = gbuffer7.Load(coord);
#endif
    
    float4 color = rt0;
#if GBUFFER_COUNT >= 2
    color += rt1;
#endif
#if GBUFFER_COUNT >= 3
    color += rt2;
#endif
#if GBUFFER_COUNT >= 4
    color += rt3;
#endif
#if GBUFFER_COUNT >= 5
    color += rt4;
#endif
#if GBUFFER_COUNT >= 6
    color += rt5;
#endif
#if GBUFFER_COUNT >= 7
    color += rt6;
#endif
#if GBUFFER_COUNT >= 8
    color += rt7;
#endif
    color *= rcp((float)GBUFFER_COUNT);

    return float4(color.rgb, 1.0f);
}
