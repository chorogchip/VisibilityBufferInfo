
cbuffer nums : register(b0)
{
    uint total_cnt;
};

StructuredBuffer<uint> src : register(t0);
RWStructuredBuffer<uint> dst_block : register(u0);
RWStructuredBuffer<uint> dst : register(u1);

static const uint WARP_SIZE_LOG = 5;
static const uint WARP_SIZE = 1 << WARP_SIZE_LOG;
static const uint THREAD_CNT = 256;
static const uint WARP_CNT = THREAD_CNT / WARP_SIZE;

groupshared uint shared_mem[WARP_CNT];

[WaveSize(WARP_SIZE)] // just..
[numthreads(THREAD_CNT, 1, 1)]
void kernel_prefix_block(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID)
{
    uint idx = gid.x * THREAD_CNT + tid.x;
    uint lane = tid.x & (WARP_SIZE - 1);
    uint warp = tid.x >> WARP_SIZE_LOG;
    
    uint val = idx < total_cnt ? src[idx] : 0;
    
    // warp 내부 prefix sum
    for (int offset = 1; offset <= WARP_SIZE / 2; offset <<= 1)
    {
        uint source_lane = lane >= offset ? lane - offset : lane;
        uint tmp = WaveReadLaneAt(val, source_lane);
        if (lane >= offset)
            val += tmp;
    }
    
    // warp sum 쓰기
    if (lane == WARP_SIZE - 1)
        shared_mem[warp] = val;
    
    GroupMemoryBarrierWithGroupSync();
    
    // 첫번째 warp만 실행
    if (warp == 0)
    {
        // warp 내부지만 warp수가 더 작거나같다 가정
        uint val_warp = lane < WARP_CNT ? shared_mem[lane] : 0;
        for (uint offset = 1; offset <= WARP_CNT / 2; offset <<= 1)
        {
            uint source_lane = lane >= offset ? lane - offset : lane;
            uint tmp = WaveReadLaneAt(val_warp, source_lane);
            if (lane >= offset)
                val_warp += tmp;
        }
        
        // block sum 쓰기
        if (lane == WARP_CNT - 1)
            dst_block[gid.x] = val_warp;
        
        // 한칸씩 땡겨서 warp별 더해줄거 저장
        uint source_lane = lane > 0 ? lane - 1 : lane;
        val_warp = WaveReadLaneAt(val_warp, source_lane);
        if (lane == 0)
            val_warp = 0;
        
        if (lane < WARP_CNT)
            shared_mem[lane] = val_warp;
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    val += shared_mem[warp];
    if (idx < total_cnt)
        dst[idx] = val;
}