#include "common_barycentric.hlsli"
#include "common_material.hlsli"
#include "common_material_data.hlsli"
#include "common_gbuffer.hlsli"

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

struct ObjectData
{
    uint instance_id;
    uint material_id;
    uint submesh_id;
    uint flags;
    float4x4 World;
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
StructuredBuffer<ObjectData> gObjects : register(t4);
StructuredBuffer<MaterialData> gMaterials : register(t5);

GBufferOutput main(PSInput input)
{
    float4 gbuffer_value = float4(0.1f, 0.1f, 0.15f, 1.0f);
    
    uint2 pixel = uint2(input.position.xy);
    uint2 vis = gVisibility.Load(int3(pixel.xy, 0));
    
    if (vis.x != 0)
    {
        uint object_id = vis.x - 1;
        uint primitive_id = vis.y;
    
        ObjectData obj = gObjects[object_id];
        Submesh submesh = gSubmeshes[obj.submesh_id];
        uint i0 = gIndices[submesh.index_offset + primitive_id * 3 + 0];
        uint i1 = gIndices[submesh.index_offset + primitive_id * 3 + 1];
        uint i2 = gIndices[submesh.index_offset + primitive_id * 3 + 2];
    
        Vertex v0 = gVertices[submesh.vertex_offset + i0];
        Vertex v1 = gVertices[submesh.vertex_offset + i1];
        Vertex v2 = gVertices[submesh.vertex_offset + i2];
    
        float4 world0 = mul(float4(v0.position, 1.0f), obj.World);
        float4 world1 = mul(float4(v1.position, 1.0f), obj.World);
        float4 world2 = mul(float4(v2.position, 1.0f), obj.World);

        float4 view0 = mul(world0, gView);
        float4 view1 = mul(world1, gView);
        float4 view2 = mul(world2, gView);

        float4 clip0 = mul(view0, gProj);
        float4 clip1 = mul(view1, gProj);
        float4 clip2 = mul(view2, gProj);
    
        float2 p0 = clip_to_pixel(clip0, gViewportSize);
        float2 p1 = clip_to_pixel(clip1, gViewportSize);
        float2 p2 = clip_to_pixel(clip2, gViewportSize);
    
        BarycentricGradient bary_grad = calc_barycentric_with_grad(
        input.position.xy, p0, p1, p2);
    
        float3 bary = bary_grad.value;
        float3 w_inv = rcp(float3(clip0.w, clip1.w, clip2.w));
    
        float D = dot(bary, w_inv);
        float D_inv = rcp(D);
    
        float3 bary_perspective = bary * w_inv * D_inv;

        AttributeGrad uv0_grad = interpolate_uv_with_grad(
        v0.uv0, v1.uv0, v2.uv0,
        w_inv, D_inv,
        bary, bary_grad.dx, bary_grad.dy);
    
        float2 uv = uv0_grad.value;
        float2 d_uv_dx = uv0_grad.dx;
        float2 d_uv_dy = uv0_grad.dy;
    
        float3 normal0 = mul(mul(float4(v0.normal, 0.0f), obj.World).xyz, (float3x3) gView);
        float3 normal1 = mul(mul(float4(v1.normal, 0.0f), obj.World).xyz, (float3x3) gView);
        float3 normal2 = mul(mul(float4(v2.normal, 0.0f), obj.World).xyz, (float3x3) gView);

        float3 normal = normalize(
        normal0 * bary_perspective.x +
        normal1 * bary_perspective.y +
        normal2 * bary_perspective.z);
    
        float4 base_color = gMaterials[obj.material_id].base_color;
        uint index = gMaterials[obj.material_id].texture_indices[0];
        gbuffer_value = float4(apply_workload_visbuf(index, uv, d_uv_dx, d_uv_dy, normal), base_color.a);
    }
    
    return make_gbuffer_output(gbuffer_value);
}
