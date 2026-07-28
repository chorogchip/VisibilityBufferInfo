#include "..\common\common_barycentric.hlsli"

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

struct Vertex
{
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv0;
};

struct Submesh
{
    uint vertex_offset;
    uint vertex_count;
    uint index_offset;
    uint index_count;
    uint material_id;
    uint padding0;
    uint padding1;
    uint padding2;
};

struct InstanceData
{
    uint instance_id;
    uint pad0;
    uint pad1;
    uint pad2;
    float4x4 World;
};

struct DrawInstanceData
{
    uint instance_id;
    uint submesh_id;
};

cbuffer MatricesCB : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float2 gViewportSize;
    float2 gInvViewportSize;
};

Texture2D<uint2> gVisibility : register(t0);
StructuredBuffer<Vertex> gVertices : register(t1);
StructuredBuffer<uint> gIndices : register(t2);
StructuredBuffer<Submesh> gSubmeshes : register(t3);
StructuredBuffer<InstanceData> gInstances : register(t4);
StructuredBuffer<DrawInstanceData> gDrawInstances : register(t5);

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

float4 main(PSInput input) : SV_Target
{
    const uint2 pixel = uint2(input.position.xy);
    const uint2 visibility = gVisibility.Load(int3(pixel, 0));
    if (visibility.x == 0)
        return float4(0.1f, 0.1f, 0.15f, 1.0f);

    const uint geometry_instance_id = visibility.x - 1;
    const uint primitive_id = visibility.y;
    const DrawInstanceData draw_instance =
        gDrawInstances[geometry_instance_id];
    const InstanceData instance = gInstances[draw_instance.instance_id];
    const Submesh submesh = gSubmeshes[draw_instance.submesh_id];

    const uint i0 = gIndices[submesh.index_offset + primitive_id * 3 + 0];
    const uint i1 = gIndices[submesh.index_offset + primitive_id * 3 + 1];
    const uint i2 = gIndices[submesh.index_offset + primitive_id * 3 + 2];
    const Vertex v0 = gVertices[submesh.vertex_offset + i0];
    const Vertex v1 = gVertices[submesh.vertex_offset + i1];
    const Vertex v2 = gVertices[submesh.vertex_offset + i2];

    const float4 clip0 = mul(
        mul(float4(v0.position, 1.0f), instance.World), gView);
    const float4 clip1 = mul(
        mul(float4(v1.position, 1.0f), instance.World), gView);
    const float4 clip2 = mul(
        mul(float4(v2.position, 1.0f), instance.World), gView);
    const float4 clip0_projected = mul(clip0, gProj);
    const float4 clip1_projected = mul(clip1, gProj);
    const float4 clip2_projected = mul(clip2, gProj);

    const float2 p0 = clip_to_pixel(clip0_projected, gViewportSize);
    const float2 p1 = clip_to_pixel(clip1_projected, gViewportSize);
    const float2 p2 = clip_to_pixel(clip2_projected, gViewportSize);
    const BarycentricGradient bary_grad = calc_barycentric_with_grad(
        input.position.xy, p0, p1, p2);
    const float3 bary = bary_grad.value;
    const float3 w_inv = rcp(float3(
        clip0_projected.w, clip1_projected.w, clip2_projected.w));
    const float D_inv = rcp(dot(bary, w_inv));
    const float3 bary_perspective = bary * w_inv * D_inv;
    const AttributeGrad uv_grad = interpolate_uv_with_grad(
        v0.uv0, v1.uv0, v2.uv0,
        w_inv, D_inv, bary, bary_grad.dx, bary_grad.dy);

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
    return encode_signed(uv_grad.dx);
#elif VISIBILITY_DEBUG_MODE == DEBUG_UV_DY
    return encode_signed(uv_grad.dy);
#else
    return float4(1.0f, 0.0f, 1.0f, 1.0f);
#endif
}
