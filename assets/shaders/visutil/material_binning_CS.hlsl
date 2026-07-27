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
StructuredBuffer<ObjectData> gObjects : register(t4);
StructuredBuffer<MaterialData> gMaterials : register(t5);
RWStructuredBuffer<uint> gBinCounts : register(u0);

groupshared uint sBinCounts[MATERIAL_BIN_COUNT];

[numthreads(BLOCK_WID, BLOCK_WID, 1)]
void kernel_material_binning(uint3 pos : SV_DispatchThreadID, uint3 grp : SV_GroupThreadID)
{
    uint ind = grp.y * BLOCK_WID + grp.x;
    
    [unroll]
    for (uint i1 = ind; i1 < MATERIAL_BIN_COUNT; i1 += THREAD_CNT)
    {
        sBinCounts[i1] = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    
    uint2 pixel = pos.xy;
    
    if (pixel.x < screen_width && pixel.y < screen_height)
    {
        uint2 vis = gVisibility.Load(int3(pixel, 0));
    
        if (vis.x != 0)
        {
            uint object_id = vis.x - 1;
    
            uint material_id = gObjects[object_id].material_id;
            uint shader_id = gMaterials[material_id].virtual_shader_id;
    
            if (shader_id < MATERIAL_BIN_COUNT)
            {
                InterlockedAdd(sBinCounts[shader_id], 1);
            }
        }
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    [unroll]
    for (uint i2 = ind; i2 < MATERIAL_BIN_COUNT; i2 += THREAD_CNT)
    {
        uint cnt = sBinCounts[i2];
        if (cnt != 0)
        {
            InterlockedAdd(gBinCounts[i2], cnt);
        }
    }
}

/*
대부분이 PBR shader라서 특정 인덱스 하나가 많다면은 interlockedadd보단 바로 prefix sum 비슷하게 가는게 나을 것이다.
사실 그냥 PBR을 compact 안하고 전체에 launch하는게 나을지도 모른다.
그리고 별개로 지금 사이즈별로 테스트해봐야 한다

일단 material수 상한 256으로 잡는다.
16x16이면 FHD 풀스크린에 8천개 타일이다
256 atomic -> 8000 atomic이면
*/
