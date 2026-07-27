cbuffer nums : register(b0)
{
    uint total_cnt;
};

StructuredBuffer<uint> src : register(t0);
RWStructuredBuffer<uint> dst : register(u0);

static const uint THREAD_CNT = 256;

groupshared uint shared_mem[THREAD_CNT];

[numthreads(THREAD_CNT, 1, 1)]
void kernel_prefix_block(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID)
{
    const uint idx = gid.x * THREAD_CNT + tid.x;

    shared_mem[tid.x] = idx < total_cnt ? src[idx] : 0;
    GroupMemoryBarrierWithGroupSync();

    for (uint offset = 1; offset < THREAD_CNT; offset <<= 1)
    {
        const uint addend = tid.x >= offset ? shared_mem[tid.x - offset] : 0;
        GroupMemoryBarrierWithGroupSync();
        if (tid.x >= offset)
        {
            shared_mem[tid.x] += addend;
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (idx < total_cnt)
    {
        dst[idx] = shared_mem[tid.x];
    }
}
