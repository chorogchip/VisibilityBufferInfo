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
        struct PendingStatsFrame {
            bool valid = false;
            std::uint64_t frame_number = 0;
            std::uint64_t triangle_count = 0;
        };

        struct StatsRow {
            std::uint64_t frame = 0;
            std::uint64_t triangle_count = 0;
            std::array<std::uint32_t, PassRasterStats::STATS_COUNTER_COUNT> counters{};
        };

        void init2_() override;
        void render_prepare_() override;
        void render_record_() override;
        void before_close_() override;

        void allocate_counter_buffers_();
        void allocate_draw_buffers_();
        std::uint64_t count_draw_chunks_(
            const std::vector<scene::SceneCPUData::DrawCall>& draws) const;
        std::uint64_t count_draw_triangles_(
            const std::vector<scene::SceneCPUData::DrawCall>& draws) const;
        void build_visible_draws_();
        void collect_completed_stats_(UINT frame_index);
        void write_stats_csv_() const;

        PassRasterStats pass_stats_;
        eng::GPUResource draw_buffer_;
        Microsoft::WRL::ComPtr<ID3D12Resource> draw_upload_buffer_;
        eng::GPUResource pixel_count_buffer_;
        eng::GPUResource stats_buffer_;
        Microsoft::WRL::ComPtr<ID3D12Resource>
            stats_readback_buffers_[util::Constants::FRAME_COUNT];

        std::vector<RasterStatsDraw> visible_draws_;
        std::uint32_t draw_capacity_ = 0;
        std::uint32_t pixel_count_ = 0;
        std::uint64_t visible_triangle_count_ = 0;
        std::uint64_t frame_number_ = 0;
        PendingStatsFrame pending_stats_[util::Constants::FRAME_COUNT]{};
        std::vector<StatsRow> stats_rows_;
    };

}
