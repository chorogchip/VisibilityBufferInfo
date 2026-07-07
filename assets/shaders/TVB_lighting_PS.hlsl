struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv;
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

cbuffer MatricesCB : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
};

Texture2D<uint2> gVisibility : register(t0);
StructuredBuffer<Vertex> gVertices : register(t1);
StructuredBuffer<uint> gIndices : register(t2);
StructuredBuffer<Mesh> gMeshes : register(t3);
StructuredBuffer<ObjectData> gObjects : register(t4);

float4 main(PSInput input) : SV_Target
{
    uint2 pixel = uint2(input.position.xy);
    uint2 vis = gVisibility.Load(int3(pixel.xy, 0));
    
    if (vis.x == 0)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    
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
    
    // TODO barycentric
    
    float c1 = (float) object_id;
    float c2 = (float) primitive_id;
    
    c1 = fmod(c1 * 21.0f / 64.0f, 1.0f);
    c2 = fmod(c2 * 51.0f / 64.0f, 1.0f);

    return float4(c1, c2, 0.0f, 1.0f);
}
