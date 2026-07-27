cbuffer nums : register(b0)
{
    uint total_cnt;
};

StructuredBuffer<uint> src : register(t0);
RWStructuredBuffer<uint> dst : register(u0);

static const uint WARP_SIZE_LOG = 5;
static const uint WARP_SIZE = 1u << WARP_SIZE_LOG;
static const uint THREAD_CNT = 256;
static const uint WARP_CNT = THREAD_CNT / WARP_SIZE;

groupshared uint shared_mem[WARP_CNT];

[WaveSize(WARP_SIZE)]
[numthreads(THREAD_CNT, 1, 1)]
void kernel_prefix_block(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID)
{
    const uint idx = gid.x * THREAD_CNT + tid.x;
    const uint lane = tid.x & (WARP_SIZE - 1);
    const uint warp = tid.x >> WARP_SIZE_LOG;

    uint val = idx < total_cnt ? src[idx] : 0;

    for (uint offset = 1; offset < WARP_SIZE; offset <<= 1)
    {
        const uint source_lane = lane >= offset ? lane - offset : lane;
        const uint tmp = WaveReadLaneAt(val, source_lane);
        if (lane >= offset)
        {
            val += tmp;
        }
    }

    if (lane == WARP_SIZE - 1)
    {
        shared_mem[warp] = val;
    }

    GroupMemoryBarrierWithGroupSync();

    if (warp == 0)
    {
        uint val_warp = lane < WARP_CNT ? shared_mem[lane] : 0;
        for (uint offset = 1; offset < WARP_CNT; offset <<= 1)
        {
            const uint source_lane = lane >= offset ? lane - offset : lane;
            const uint tmp = WaveReadLaneAt(val_warp, source_lane);
            if (lane >= offset)
            {
                val_warp += tmp;
            }
        }

        const uint source_lane = lane > 0 ? lane - 1 : lane;
        val_warp = WaveReadLaneAt(val_warp, source_lane);
        if (lane == 0)
        {
            val_warp = 0;
        }

        if (lane < WARP_CNT)
        {
            shared_mem[lane] = val_warp;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    val += shared_mem[warp];
    if (idx < total_cnt)
    {
        dst[idx] = val;
    }
}
