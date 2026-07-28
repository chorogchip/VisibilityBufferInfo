

#define MATERIAL_BIN_COUNT 256

RWStructuredBuffer<uint> gBinCounts : register(u0);

[numthreads(MATERIAL_BIN_COUNT, 1, 1)]
void kernel_clear_counts(uint3 tid : SV_GroupThreadID)
{
    gBinCounts[tid.x] = 0;
}
