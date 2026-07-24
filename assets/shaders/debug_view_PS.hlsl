#include "common_material_data.hlsli"
#include "common_input_struct.hlsli"

StructuredBuffer<MaterialData> gMaterials : register(t1);

#define DEBUG_VIEW 12

#define DEBUG_VIEW_NONE             0
#define DEBUG_VIEW_NORMAL           1
#define DEBUG_VIEW_TANGENT          2
#define DEBUG_VIEW_TEXCOORD0        3
#define DEBUG_VIEW_TEXCOORD1        4
#define DEBUG_VIEW_WORLD_POSITION   5
#define DEBUG_VIEW_MATERIAL_ID      6
#define DEBUG_VIEW_INSTANCE_ID      7
#define DEBUG_VIEW_OBJECT_ID        8
#define DEBUG_VIEW_MESH_ID          9
#define DEBUG_VIEW_FLAGS           10
#define DEBUG_VIEW_VERTEX_ID       11
#define DEBUG_VIEW_TRIANGLE_ID     12
#define DEBUG_VIEW_FRONT_FACE      13

#ifndef DEBUG_VIEW
#define DEBUG_VIEW DEBUG_VIEW_NONE
#endif

struct DebugPSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float3 tangent : TANGENT;
    float3 world_pos : WORLDPOS;

    nointerpolation uint material_index : MATERIAL;
    nointerpolation uint instance_id : INSTANCE;
    nointerpolation uint object_id : OBJECT;
    nointerpolation uint mesh_index : MESH;
    nointerpolation uint flags : FLAGS;
    nointerpolation uint vertex_id : VERTEXID;
};

float3 HashColor(uint value)
{
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;

    return float3(
        value & 0xFFu,
        (value >> 8) & 0xFFu,
        (value >> 16) & 0xFFu
    ) / 255.0f;
}

float4 main(
    DebugPSInput input,
    uint primitiveID : SV_PrimitiveID,
    bool isFrontFace : SV_IsFrontFace
) : SV_TARGET
{
#if DEBUG_VIEW == DEBUG_VIEW_NORMAL

    return float4(normalize(input.normal) * 0.5f + 0.5f, 1.0f);

#elif DEBUG_VIEW == DEBUG_VIEW_TANGENT

    return float4(normalize(input.tangent) * 0.5f + 0.5f, 1.0f);

#elif DEBUG_VIEW == DEBUG_VIEW_TEXCOORD0

    return float4(frac(input.texcoord0), 0.0f, 1.0f);

#elif DEBUG_VIEW == DEBUG_VIEW_TEXCOORD1

    return float4(frac(input.texcoord1), 0.0f, 1.0f);

#elif DEBUG_VIEW == DEBUG_VIEW_WORLD_POSITION

    return float4(frac(abs(input.world_pos)), 1.0f);

#elif DEBUG_VIEW == DEBUG_VIEW_MATERIAL_ID

    return float4(HashColor(input.material_index), 1.0f);

#elif DEBUG_VIEW == DEBUG_VIEW_INSTANCE_ID

    return float4(HashColor(input.instance_id), 1.0f);

#elif DEBUG_VIEW == DEBUG_VIEW_OBJECT_ID

    return float4(HashColor(input.object_id), 1.0f);

#elif DEBUG_VIEW == DEBUG_VIEW_MESH_ID

    return float4(HashColor(input.mesh_index), 1.0f);

#elif DEBUG_VIEW == DEBUG_VIEW_FLAGS

    return float4(HashColor(input.flags), 1.0f);

#elif DEBUG_VIEW == DEBUG_VIEW_VERTEX_ID

    return float4(HashColor(input.vertex_id), 1.0f);

#elif DEBUG_VIEW == DEBUG_VIEW_TRIANGLE_ID

    return float4(HashColor(primitiveID), 1.0f);

#elif DEBUG_VIEW == DEBUG_VIEW_FRONT_FACE

    return isFrontFace
        ? float4(0.0f, 1.0f, 0.0f, 1.0f)
        : float4(1.0f, 0.0f, 0.0f, 1.0f);

#else

    return float4(1.0f, 0.0f, 0.0f, 1.0f);

#endif
}