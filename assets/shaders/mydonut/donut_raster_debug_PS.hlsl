#include "..\common\mydonut_scene_abi.hlsli"

#ifndef VISIBILITY_DEBUG_MODE
#define VISIBILITY_DEBUG_MODE 3
#endif

#define DEBUG_BARYCENTRIC             3
#define DEBUG_PERSPECTIVE_BARYCENTRIC 4
#define DEBUG_BARYCENTRIC_DX          5
#define DEBUG_BARYCENTRIC_DY          6
#define DEBUG_UV_DX                   7
#define DEBUG_UV_DY                   8
#define DEBUG_UV_LOD_PROXY            9

struct PSInput
{
    float4 clipPosition : SV_Position;
    float2 texCoord : TEXCOORD0;
    nointerpolation uint geometryInstanceID : TEXCOORD1;
#if VISIBILITY_DEBUG_MODE == DEBUG_BARYCENTRIC || \
    VISIBILITY_DEBUG_MODE == DEBUG_BARYCENTRIC_DX || \
    VISIBILITY_DEBUG_MODE == DEBUG_BARYCENTRIC_DY
    noperspective float3 barycentrics : SV_Barycentrics;
#elif VISIBILITY_DEBUG_MODE == DEBUG_PERSPECTIVE_BARYCENTRIC
    float3 barycentrics : SV_Barycentrics;
#endif
};

float4 encode_signed(float3 value)
{
    return float4(saturate(0.5f + value * 16.0f), 1.0f);
}

float4 encode_signed(float2 value)
{
    return float4(
        saturate(0.5f + value.x * 16.0f),
        saturate(0.5f + value.y * 16.0f),
        0.5f,
        1.0f);
}

float encode_lod_proxy(float2 uv_dx, float2 uv_dy)
{
    const float reference_texture_size = 1024.0f;
    const float rho = max(length(uv_dx), length(uv_dy)) *
        reference_texture_size;
    const float lod = log2(max(rho, 1e-8f));
    return saturate((lod + 8.0f) / 24.0f);
}

float4 main(PSInput input) : SV_Target
{
#if VISIBILITY_DEBUG_MODE == DEBUG_BARYCENTRIC
    return float4(input.barycentrics, 1.0f);
#elif VISIBILITY_DEBUG_MODE == DEBUG_PERSPECTIVE_BARYCENTRIC
    return float4(input.barycentrics, 1.0f);
#elif VISIBILITY_DEBUG_MODE == DEBUG_BARYCENTRIC_DX
    return encode_signed(ddx_coarse(input.barycentrics));
#elif VISIBILITY_DEBUG_MODE == DEBUG_BARYCENTRIC_DY
    return encode_signed(ddy_coarse(input.barycentrics));
#elif VISIBILITY_DEBUG_MODE == DEBUG_UV_DX
    return encode_signed(ddx_coarse(input.texCoord));
#elif VISIBILITY_DEBUG_MODE == DEBUG_UV_DY
    return encode_signed(ddy_coarse(input.texCoord));
#elif VISIBILITY_DEBUG_MODE == DEBUG_UV_LOD_PROXY
    const float2 uv_dx = ddx_coarse(input.texCoord);
    const float2 uv_dy = ddy_coarse(input.texCoord);
    const float encoded = encode_lod_proxy(uv_dx, uv_dy);
    return float4(encoded, encoded, encoded, 1.0f);
#else
    return float4(1.0f, 0.0f, 1.0f, 1.0f);
#endif
}
