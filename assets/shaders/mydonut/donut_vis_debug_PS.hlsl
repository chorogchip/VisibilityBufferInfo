#include "..\common\common_barycentric.hlsli"
#include "..\common\mydonut_scene_abi.hlsli"

#ifndef VISIBILITY_DEBUG_MODE
#define VISIBILITY_DEBUG_MODE 0
#endif

#define DEBUG_GEOMETRY_INSTANCE_HASH 0
#define DEBUG_PRIMITIVE_HASH         1
#define DEBUG_GEOMETRY_PRIMITIVE_HASH 2
#define DEBUG_BARYCENTRIC            3
#define DEBUG_PERSPECTIVE_BARYCENTRIC 4
#define DEBUG_BARYCENTRIC_DX         5
#define DEBUG_BARYCENTRIC_DY         6
#define DEBUG_UV_DX                  7
#define DEBUG_UV_DY                  8

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer c_VertexLayout : register(b1, space1)
{
    uint g_PositionOffset;
    uint g_TexCoordOffset;
};

cbuffer c_GBuffer : register(b2, space2)
{
    GBufferFillConstants g_GBuffer;
};

Texture2D<uint2> t_Visibility :
    register(t20, space1);
ByteAddressBuffer t_Indices :
    register(t21, space1);
ByteAddressBuffer t_Vertices :
    register(t22, space1);
StructuredBuffer<InstanceData> t_Instances :
    register(t23, space1);
StructuredBuffer<SubmeshData> t_Submeshes :
    register(t24, space1);
StructuredBuffer<GeometryInstanceData> t_GeometryInstances :
    register(t25, space1);

float3 hash_color(uint value)
{
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;

    return float3(
        value & 0xFFu,
        (value >> 8) & 0xFFu,
        (value >> 16) & 0xFFu) / 255.0f;
}

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

void fetch_vertex(uint vertex_index, out float3 position, out float2 texcoord)
{
    position = asfloat(t_Vertices.Load3(
        g_PositionOffset + vertex_index * SizeOfPosition));
    texcoord = asfloat(t_Vertices.Load2(
        g_TexCoordOffset + vertex_index * SizeOfTexcoord));
}

float4 main(PSInput input) : SV_Target
{
    const uint2 pixel = uint2(input.position.xy);
    const uint2 visibility = t_Visibility.Load(int3(pixel, 0));
    if (visibility.x == 0)
        return float4(0.1f, 0.1f, 0.15f, 1.0f);

    const uint geometry_instance_id = visibility.x - 1;
    const uint primitive_id = visibility.y;
    const GeometryInstanceData geometry =
        t_GeometryInstances[geometry_instance_id];
    const SubmeshData submesh = t_Submeshes[geometry.submeshID];
    const InstanceData instance = t_Instances[geometry.instanceID];

    const uint index_offset = submesh.indexOffset + primitive_id * 3;
    const uint index0 = t_Indices.Load((index_offset + 0) * 4);
    const uint index1 = t_Indices.Load((index_offset + 1) * 4);
    const uint index2 = t_Indices.Load((index_offset + 2) * 4);

    float3 position0, position1, position2;
    float2 texcoord0, texcoord1, texcoord2;
    fetch_vertex(index0, position0, texcoord0);
    fetch_vertex(index1, position1, texcoord1);
    fetch_vertex(index2, position2, texcoord2);

    const float4 clip0 = mul(float4(
        TransformPoint(instance.transform, position0), 1.0f),
        g_GBuffer.view.matWorldToClip);
    const float4 clip1 = mul(float4(
        TransformPoint(instance.transform, position1), 1.0f),
        g_GBuffer.view.matWorldToClip);
    const float4 clip2 = mul(float4(
        TransformPoint(instance.transform, position2), 1.0f),
        g_GBuffer.view.matWorldToClip);

    const float2 pixel0 = clip_to_pixel(clip0, g_GBuffer.view.viewportSize);
    const float2 pixel1 = clip_to_pixel(clip1, g_GBuffer.view.viewportSize);
    const float2 pixel2 = clip_to_pixel(clip2, g_GBuffer.view.viewportSize);
    const BarycentricGradient bary_grad = calc_barycentric_with_grad(
        float2(pixel) + float2(0.5f, 0.5f), pixel0, pixel1, pixel2);
    const float3 bary = bary_grad.value;
    const float3 inv_w = rcp(float3(clip0.w, clip1.w, clip2.w));
    const float inv_d = rcp(dot(bary, inv_w));
    const float3 bary_perspective = bary * inv_w * inv_d;
    const AttributeGrad texcoord_grad = interpolate_uv_with_grad(
        texcoord0, texcoord1, texcoord2,
        inv_w, inv_d, bary, bary_grad.dx, bary_grad.dy);

#if VISIBILITY_DEBUG_MODE == DEBUG_GEOMETRY_INSTANCE_HASH
    return float4(hash_color(geometry_instance_id), 1.0f);
#elif VISIBILITY_DEBUG_MODE == DEBUG_PRIMITIVE_HASH
    return float4(hash_color(primitive_id), 1.0f);
#elif VISIBILITY_DEBUG_MODE == DEBUG_GEOMETRY_PRIMITIVE_HASH
    return float4(hash_color(
        geometry_instance_id ^ (primitive_id * 0x9E3779B9u)), 1.0f);
#elif VISIBILITY_DEBUG_MODE == DEBUG_BARYCENTRIC
    return float4(bary, 1.0f);
#elif VISIBILITY_DEBUG_MODE == DEBUG_PERSPECTIVE_BARYCENTRIC
    return float4(bary_perspective, 1.0f);
#elif VISIBILITY_DEBUG_MODE == DEBUG_BARYCENTRIC_DX
    return encode_signed(bary_grad.dx);
#elif VISIBILITY_DEBUG_MODE == DEBUG_BARYCENTRIC_DY
    return encode_signed(bary_grad.dy);
#elif VISIBILITY_DEBUG_MODE == DEBUG_UV_DX
    return encode_signed(texcoord_grad.dx);
#elif VISIBILITY_DEBUG_MODE == DEBUG_UV_DY
    return encode_signed(texcoord_grad.dy);
#else
    return float4(1.0f, 0.0f, 1.0f, 1.0f);
#endif
}
