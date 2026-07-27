#include "../common_material_data.hlsli"

struct ObjectData
{
    uint instance_id;
    uint material_id;
    uint submesh_id;
    uint flags;
    float4x4 World;
};

cbuffer nums : register(b0)
{
    uint screen_width;
    uint screen_height;
}

#ifndef MATERIAL_BIN_COUNT
#define MATERIAL_BIN_COUNT 256
#elif MATERIAL_BIN_COUNT != 256
#error Current material binning implementation requires MATERIAL_BIN_COUNT == 256
#endif

#ifndef BLOCK_WID
#define BLOCK_WID 16
#endif

#define THREAD_CNT  (BLOCK_WID * BLOCK_WID)

Texture2D<uint2> gVisibility : register(t0);
StructuredBuffer<uint> gBinCountsPrefix : register(t1);
StructuredBuffer<ObjectData> gObjects : register(t4);
StructuredBuffer<MaterialData> gMaterials : register(t5);

RWStructuredBuffer<uint> gBinCounts : register(u0);
RWStructuredBuffer<uint2> gFinalPos : register(u1);

groupshared uint sBinCounts[MATERIAL_BIN_COUNT];
groupshared uint sBinOffset[MATERIAL_BIN_COUNT];

[numthreads(BLOCK_WID, BLOCK_WID, 1)]
void kernel_material_flatten(uint3 pos : SV_DispatchThreadID, uint3 grp : SV_GroupThreadID)
{
    uint ind = grp.y * BLOCK_WID + grp.x;
    
    [unroll]
    for (uint i1 = ind; i1 < MATERIAL_BIN_COUNT; i1 += THREAD_CNT)
    {
        sBinCounts[i1] = 0;
        sBinOffset[i1] = i1 == 0 ? 0 : gBinCountsPrefix[i1 - 1];
        // or sBinOffset[i1] = 0 and dont use sBinOffset below, can reduce memory traffic in 8x8
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    uint2 pixel = pos.xy;
    uint final_offset = 0;
    uint shader_id = 0;
    bool valid = false;
    
    if (pixel.x < screen_width && pixel.y < screen_height)
    {
        uint2 vis = gVisibility.Load(int3(pixel, 0));
    
        if (vis.x != 0)
        {
            uint object_id = vis.x - 1;
            uint material_id = gObjects[object_id].material_id;
    
            shader_id = gMaterials[material_id].virtual_shader_id;
            valid = shader_id < MATERIAL_BIN_COUNT;
    
            if (valid)
            {
                uint offset_in_tile;
                InterlockedAdd(sBinCounts[shader_id], 1, offset_in_tile);
                uint prefix_value = sBinOffset[shader_id];
                final_offset = offset_in_tile + prefix_value;
            }

        }
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    [unroll]
    for (uint i2 = ind; i2 < MATERIAL_BIN_COUNT; i2 += THREAD_CNT)
    {
        uint cnt = sBinCounts[i2];
        uint offset_of_tile = 0;
        if (cnt != 0)
        {
            InterlockedAdd(gBinCounts[i2], cnt, offset_of_tile);
            sBinOffset[i2] = offset_of_tile;
        }
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    if (valid)
    {
        final_offset += sBinOffset[shader_id];
        gFinalPos[final_offset] = pixel;
    }
}
