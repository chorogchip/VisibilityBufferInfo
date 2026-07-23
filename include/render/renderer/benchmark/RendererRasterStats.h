#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <wrl.h>

#include "engine/GPUResource.h"
#include "render/pass/benchmark/PassRasterStats.h"
#include "render/renderer/benchmark/RendererBenchmark.h"

namespace rndr {

    class RendererRasterStats : public RendererBenchmark {
    public:
        ~RendererRasterStats() override = default;

    private:
        struct RasterStatsTriangle {
            std::uint32_t object_index = 0;
            std::uint32_t i0 = 0;
            std::uint32_t i1 = 0;
            std::uint32_t i2 = 0;
        };
        static_assert(sizeof(RasterStatsTriangle) ==
            PassRasterStats::TRIANGLE_STRIDE_BYTES);

        struct PendingStatsFrame {
            bool valid = false;
            std::uint64_t frame_number = 0;
            std::uint32_t triangle_count = 0;
        };

        struct StatsRow {
            std::uint64_t frame = 0;
            std::uint32_t triangle_count = 0;
            std::array<std::uint32_t, PassRasterStats::STATS_COUNTER_COUNT> counters{};
        };

        void init2_() override;
        void render_prepare_() override;
        void render_record_() override;
        void before_close_() override;

        void allocate_counter_buffers_();
        void allocate_triangle_buffers_();
        std::uint64_t count_batch_triangles_(
            const std::vector<scene::SceneDataCPU::ObjectBatch>& batches) const;
        void build_visible_triangles_();
        void collect_completed_stats_(UINT frame_index);
        void write_stats_csv_() const;

        PassRasterStats pass_stats_;
        eng::GPUResource triangle_buffer_;
        Microsoft::WRL::ComPtr<ID3D12Resource> triangle_upload_buffer_;
        eng::GPUResource pixel_count_buffer_;
        eng::GPUResource stats_buffer_;
        Microsoft::WRL::ComPtr<ID3D12Resource>
            stats_readback_buffers_[util::Constants::FRAME_COUNT];

        std::vector<RasterStatsTriangle> visible_triangles_;
        std::uint32_t triangle_capacity_ = 0;
        std::uint32_t pixel_count_ = 0;
        std::uint64_t frame_number_ = 0;
        PendingStatsFrame pending_stats_[util::Constants::FRAME_COUNT]{};
        std::vector<StatsRow> stats_rows_;
    };

}
