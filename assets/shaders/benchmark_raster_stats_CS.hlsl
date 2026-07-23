cbuffer MatricesCB : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float2 gViewportSize;
};

cbuffer RasterStatsCB : register(b1)
{
    uint gTriangleCount;
    uint gWidth;
    uint gHeight;
    uint gPixelCount;
    uint gCountGroupCountX;
    uint3 gPadding;
};

struct RasterStatsTriangle
{
    uint object_index;
    uint i0;
    uint i1;
    uint i2;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float2 texcoord0;
    float2 texcoord1;
    float3 tangent;
};

struct InstanceData
{
    uint object_id;
    uint material_index;
    uint mesh_index;
    uint flags;
    float4x4 World;
};

StructuredBuffer<RasterStatsTriangle> gTriangles : register(t0);
StructuredBuffer<Vertex> gVertices : register(t1);
StructuredBuffer<InstanceData> gInstances : register(t2);
RWStructuredBuffer<uint> gPixelCounts : register(u0);
RWStructuredBuffer<uint> gStats : register(u1);

static const uint STAT_TOTAL_FRAGMENTS = 0;
static const uint STAT_COVERED_PIXELS = 1;
static const uint STAT_OVERDRAW_EXTRA = 2;
static const uint STAT_MAX_OVERDRAW = 3;
static const uint STAT_RASTERIZED_TRIANGLES = 4;
static const uint STAT_SKIPPED_TRIANGLES = 5;
static const uint STAT_QUAD_INSTANCES = 6;
static const uint STAT_QUAD_COVERED_LANES = 7;
static const uint STAT_QUAD_WASTE_LANES = 8;
static const uint STAT_COUNTER_COUNT = 16;

float edge2d(float2 a, float2 b, float2 p)
{
    return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
}

bool point_in_triangle(float2 p, float2 v0, float2 v1, float2 v2, float area)
{
    const float sign = area < 0.0f ? -1.0f : 1.0f;
    const float e0 = edge2d(v0, v1, p) * sign;
    const float e1 = edge2d(v1, v2, p) * sign;
    const float e2 = edge2d(v2, v0, p) * sign;
    return e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f;
}

float4 project_position(uint vertex_index, uint object_index)
{
    const float3 local_pos = gVertices[vertex_index].position;
    const InstanceData instance_data = gInstances[object_index];
    const float4 world_pos = mul(float4(local_pos, 1.0f), instance_data.World);
    const float4 view_pos = mul(world_pos, gView);
    return mul(view_pos, gProj);
}

float2 clip_to_screen(float4 clip_pos)
{
    const float2 ndc = clip_pos.xy / clip_pos.w;
    return float2(
        (ndc.x * 0.5f + 0.5f) * (float)gWidth,
        (-ndc.y * 0.5f + 0.5f) * (float)gHeight);
}

[numthreads(16, 16, 1)]
void clear_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint linear_index = dispatch_thread_id.y * gWidth + dispatch_thread_id.x;
    if (dispatch_thread_id.x < gWidth && dispatch_thread_id.y < gHeight)
        gPixelCounts[linear_index] = 0;

    if (linear_index < STAT_COUNTER_COUNT)
        gStats[linear_index] = 0;
}

[numthreads(64, 1, 1)]
void count_main(
    uint3 group_id : SV_GroupID,
    uint3 group_thread_id : SV_GroupThreadID)
{
    const uint group_linear = group_id.y * gCountGroupCountX + group_id.x;
    const uint triangle_index = group_linear * 64 + group_thread_id.x;
    if (triangle_index >= gTriangleCount)
        return;

    const RasterStatsTriangle triangle = gTriangles[triangle_index];

    const float4 clip0 = project_position(triangle.i0, triangle.object_index);
    const float4 clip1 = project_position(triangle.i1, triangle.object_index);
    const float4 clip2 = project_position(triangle.i2, triangle.object_index);

    if (clip0.w <= 0.0f || clip1.w <= 0.0f || clip2.w <= 0.0f) {
        InterlockedAdd(gStats[STAT_SKIPPED_TRIANGLES], 1);
        return;
    }

    const float2 p0 = clip_to_screen(clip0);
    const float2 p1 = clip_to_screen(clip1);
    const float2 p2 = clip_to_screen(clip2);
    const float area = edge2d(p0, p1, p2);

    // Matches the current benchmark PSO: back-face culling with clockwise
    // front faces in screen space.
    if (area >= -1.0e-5f) {
        InterlockedAdd(gStats[STAT_SKIPPED_TRIANGLES], 1);
        return;
    }

    const float2 min_p = min(p0, min(p1, p2));
    const float2 max_p = max(p0, max(p1, p2));
    const int min_x = max(0, (int)floor(min_p.x));
    const int min_y = max(0, (int)floor(min_p.y));
    const int max_x = min((int)gWidth - 1, (int)ceil(max_p.x));
    const int max_y = min((int)gHeight - 1, (int)ceil(max_p.y));

    if (min_x > max_x || min_y > max_y) {
        InterlockedAdd(gStats[STAT_SKIPPED_TRIANGLES], 1);
        return;
    }

    uint local_fragment_count = 0;
    uint local_quad_instances = 0;
    uint local_quad_covered_lanes = 0;
    uint local_quad_waste_lanes = 0;

    [loop]
    for (int y = min_y; y <= max_y; ++y) {
        [loop]
        for (int x = min_x; x <= max_x; ++x) {
            const float2 sample_pos = float2((float)x + 0.5f, (float)y + 0.5f);
            if (!point_in_triangle(sample_pos, p0, p1, p2, area))
                continue;

            InterlockedAdd(gPixelCounts[(uint)y * gWidth + (uint)x], 1);
            ++local_fragment_count;
        }
    }

    const int min_quad_x = min_x / 2;
    const int min_quad_y = min_y / 2;
    const int max_quad_x = max_x / 2;
    const int max_quad_y = max_y / 2;

    [loop]
    for (int qy = min_quad_y; qy <= max_quad_y; ++qy) {
        [loop]
        for (int qx = min_quad_x; qx <= max_quad_x; ++qx) {
            uint active_lanes = 0;
            uint covered_lanes = 0;

            [unroll]
            for (int lane_y = 0; lane_y < 2; ++lane_y) {
                [unroll]
                for (int lane_x = 0; lane_x < 2; ++lane_x) {
                    const int x = qx * 2 + lane_x;
                    const int y = qy * 2 + lane_y;
                    if (x < 0 || y < 0 ||
                        x >= (int)gWidth || y >= (int)gHeight)
                        continue;

                    ++active_lanes;
                    const float2 sample_pos =
                        float2((float)x + 0.5f, (float)y + 0.5f);
                    if (point_in_triangle(sample_pos, p0, p1, p2, area))
                        ++covered_lanes;
                }
            }

            if (covered_lanes > 0) {
                ++local_quad_instances;
                local_quad_covered_lanes += covered_lanes;
                local_quad_waste_lanes += active_lanes - covered_lanes;
            }
        }
    }

    if (local_fragment_count > 0) {
        InterlockedAdd(gStats[STAT_RASTERIZED_TRIANGLES], 1);
        InterlockedAdd(gStats[STAT_QUAD_INSTANCES], local_quad_instances);
        InterlockedAdd(gStats[STAT_QUAD_COVERED_LANES], local_quad_covered_lanes);
        InterlockedAdd(gStats[STAT_QUAD_WASTE_LANES], local_quad_waste_lanes);
    } else {
        InterlockedAdd(gStats[STAT_SKIPPED_TRIANGLES], 1);
    }
}

[numthreads(16, 16, 1)]
void reduce_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= gWidth || dispatch_thread_id.y >= gHeight)
        return;

    const uint pixel_index = dispatch_thread_id.y * gWidth + dispatch_thread_id.x;
    const uint pixel_count = gPixelCounts[pixel_index];
    if (pixel_count == 0)
        return;

    InterlockedAdd(gStats[STAT_TOTAL_FRAGMENTS], pixel_count);
    InterlockedAdd(gStats[STAT_COVERED_PIXELS], 1);
    InterlockedAdd(gStats[STAT_OVERDRAW_EXTRA], pixel_count - 1);
    InterlockedMax(gStats[STAT_MAX_OVERDRAW], pixel_count);
}
