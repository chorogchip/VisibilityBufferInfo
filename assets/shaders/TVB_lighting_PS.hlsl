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
    float2 uv : TEXCOORD;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv0;
    float2 uv1;
    float3 tangent;
};

struct Mesh
{
    uint vertex_start;
    uint index_start;
};

struct ObjectData
{
    uint object_id;
    uint material_index;
    uint mesh_index;
    uint flags;
    float4x4 World;
};

struct MaterialData
{
    float4 base_color;
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
StructuredBuffer<Mesh> gMeshes : register(t3);
StructuredBuffer<ObjectData> gObjects : register(t4);
StructuredBuffer<MaterialData> gMaterials : register(t5);

#if TEXTURE_COUNT > 0
Texture2D<float4> gTextures[TEXTURE_COUNT] : register(t8);
#endif

float2 clip_to_pixel(float4 clip_pos)
{
    float2 ndc = clip_pos.xy / clip_pos.w;
    return float2(
        (ndc.x * 0.5f + 0.5f) * gViewportSize.x,
        (0.5f - ndc.y * 0.5f) * gViewportSize.y);
}

float edge_function(float2 p0, float2 p1, float2 p2)
{
    float2 e1 = p1 - p0;
    float2 e2 = p2 - p0;
    return e2.x * e1.y - e2.y * e1.x;
}

float3 calc_barycentric(float2 p, float2 p0, float2 p1, float2 p2)
{
    float area = edge_function(p0, p1, p2);
    float area_inv = rcp(area);
    return float3(
        edge_function(p1, p2, p) * area_inv,
        edge_function(p2, p0, p) * area_inv,
        edge_function(p0, p1, p) * area_inv);
}

struct BarycentricGradient
{
    float3 value;
    float3 dx;
    float3 dy;
};

BarycentricGradient calc_barycentric_with_grad(
    float2 p, float2 p0, float2 p1, float2 p2)
{
    float2 e1 = p1 - p0;
    float2 e2 = p2 - p0;
    float2 d = p - p0;
    
    float det = e1.x * e2.y - e1.y * e2.x;
    if (abs(det) < 1e-8f) det = 1e-8f;
    float det_inv = rcp(det);
    
    float lambda1 = (d.x * e2.y - d.y * e2.x) * det_inv;
    float lambda2 = (d.y * e1.x - d.x * e1.y) * det_inv;
    float lambda0 = 1.0f - lambda1 - lambda2;
    
    float2 grad_lambda1 = float2(e2.y, -e2.x) * det_inv;
    float2 grad_lambda2 = float2(-e1.y, e1.x) * det_inv;
    float2 grad_lambda0 = -grad_lambda1 - grad_lambda2;
    
    BarycentricGradient result;
    result.value = float3(lambda0, lambda1, lambda2);
    result.dx = float3(grad_lambda0.x, grad_lambda1.x, grad_lambda2.x);
    result.dy = float3(grad_lambda0.y, grad_lambda1.y, grad_lambda2.y);

    return result;
}

struct AttributeGrad
{
    float2 value;
    float2 dx;
    float2 dy;
};

AttributeGrad interpolate_uv_with_grad(
    float2 uv0, float2 uv1, float2 uv2,
    float3 q, float inv_D,
    float3 lambda, float3 d_lambda_dx, float3 d_lambda_dy)
{   
    float2 N = lambda.x * uv0 * q.x + lambda.y * uv1 * q.y + lambda.z * uv2 * q.z;
    float2 uv = N * inv_D;
    
    float Dx = dot(d_lambda_dx, q);
    float Dy = dot(d_lambda_dy, q);
    
    float2 Nx = d_lambda_dx.x * uv0 * q.x + d_lambda_dx.y * uv1 * q.y + d_lambda_dx.z * uv2 * q.z;
    float2 Ny = d_lambda_dy.x * uv0 * q.x + d_lambda_dy.y * uv1 * q.y + d_lambda_dy.z * uv2 * q.z;
    
    AttributeGrad res;
    res.value = uv;
    res.dx = (Nx - uv * Dx) * inv_D;
    res.dy = (Ny - uv * Dy) * inv_D;
    
    return res;
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

float4 main(PSInput input) : SV_Target
{
    uint2 pixel = uint2(input.position.xy);
    uint2 vis = gVisibility.Load(int3(pixel.xy, 0));
    
    if (vis.x == 0) return float4(0.1f, 0.1f, 0.15f, 1.0f);
    
    uint object_id = vis.x - 1;
    uint primitive_id = vis.y;
    
    ObjectData obj = gObjects[object_id];
    Mesh mesh = gMeshes[obj.mesh_index];
    uint i0 = gIndices[mesh.index_start + primitive_id * 3 + 0];
    uint i1 = gIndices[mesh.index_start + primitive_id * 3 + 1];
    uint i2 = gIndices[mesh.index_start + primitive_id * 3 + 2];
    
    Vertex v0 = gVertices[mesh.vertex_start + i0];
    Vertex v1 = gVertices[mesh.vertex_start + i1];
    Vertex v2 = gVertices[mesh.vertex_start + i2];
    
    float4 world0 = mul(float4(v0.position, 1.0f), obj.World);
    float4 world1 = mul(float4(v1.position, 1.0f), obj.World);
    float4 world2 = mul(float4(v2.position, 1.0f), obj.World);

    float4 view0 = mul(world0, gView);
    float4 view1 = mul(world1, gView);
    float4 view2 = mul(world2, gView);

    float4 clip0 = mul(view0, gProj);
    float4 clip1 = mul(view1, gProj);
    float4 clip2 = mul(view2, gProj);
    
    float2 p0 = clip_to_pixel(clip0);
    float2 p1 = clip_to_pixel(clip1);
    float2 p2 = clip_to_pixel(clip2);
    
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

    float4 base_color = gMaterials[obj.material_index].base_color;
    float3 color = base_color.rgb * (normal.z * 0.5f + 0.5f);
    return float4(apply_workload(color, input.position.xy), base_color.a);
}
