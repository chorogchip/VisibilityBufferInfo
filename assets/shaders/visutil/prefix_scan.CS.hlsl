cbuffer nums : register(b0)
{
    uint total_cnt;
};

StructuredBuffer<uint> src : register(t0);
RWStructuredBuffer<uint> dst : register(u0);
RWByteAddressBuffer indirects : register(u1);

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
        uint sum = shared_mem[tid.x];
        uint org = src[idx];
        dst[idx] = sum;
        indirects.Store2(idx * 4, uint2(sum - org, org));
        indirects.Store3(idx * 4 + 2, uint3(org, 1, 1));
    }
    else
    {
        indirects.Store2(idx * 4, uint2(0, 0));
        indirects.Store3(idx * 4 + 2, uint3(0, 0, 0));
    }
}
