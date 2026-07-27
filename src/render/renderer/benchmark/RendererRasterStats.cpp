#include "render/renderer/benchmark/RendererRasterStats.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

#include "dx_util/ResourceUtils.h"
#include "util/Logger.h"
#include "util/Utils.h"
#include "util/minmax_remover.h"

namespace rndr {

    namespace {

        enum StatsCounter : std::size_t {
            TOTAL_FRAGMENTS = 0,
            COVERED_PIXELS = 1,
            OVERDRAW_EXTRA = 2,
            MAX_OVERDRAW = 3,
            RASTERIZED_TRIANGLES = 4,
            SKIPPED_TRIANGLES = 5,
            QUAD_INSTANCES = 6,
            QUAD_COVERED_LANES = 7,
            QUAD_WASTE_LANES = 8,
        };

        Microsoft::WRL::ComPtr<ID3D12Resource> create_uav_buffer(
            ID3D12Device* device,
            std::uint64_t size_in_bytes) {

            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = (std::max<std::uint64_t>)(size_in_bytes, sizeof(std::uint32_t));
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            return dxutl::create_committed_resource(
                device,
                desc,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }

        std::filesystem::path make_stats_output_path(
            const util::ProgramArgument& arguments) {

            std::filesystem::path path = arguments.output_filepath;
            const std::filesystem::path parent = path.parent_path();
            const std::string stem = path.stem().string().empty()
                ? "result"
                : path.stem().string();
            const std::string extension = path.extension().string().empty()
                ? ".csv"
                : path.extension().string();

            return parent / (
                stem + "_" + std::to_string(arguments.run_id) +
                "_raster_stats" + extension);
        }

        double safe_ratio(std::uint32_t numerator, std::uint32_t denominator) {
            if (denominator == 0) return 0.0;
            return static_cast<double>(numerator) /
                static_cast<double>(denominator);
        }
    }

    void RendererRasterStats::init2_() {
        program_result_.renderer_name = "BenchmarkRasterStats";
        program_result_.pass_names[1] = "raster_stats";

        this->allocate_counter_buffers_();
        this->allocate_draw_buffers_();

        PassRasterStatsResources resources{};
        resources.constant_buffer_addresses[0] =
            buf_constant_[0].get()->GetGPUVirtualAddress();
        resources.constant_buffer_addresses[1] =
            buf_constant_[1].get()->GetGPUVirtualAddress();
        static_assert(util::Constants::FRAME_COUNT == 2);
        resources.draw_buffer = &draw_buffer_;
        resources.draw_upload_buffer = draw_upload_buffer_.Get();
        resources.index_buffer = &scene_index_buffer_;
        resources.vertex_buffer = &scene_vertex_buffer_;
        resources.instance_buffer = &scene_instance_buffer_;
        resources.draw_instance_buffer = &scene_draw_instance_buffer_;
        resources.draw_instance_id_buffer = &scene_draw_instance_id_buffer_;
        resources.pixel_count_buffer = &pixel_count_buffer_;
        resources.stats_buffer = &stats_buffer_;
        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i)
            resources.stats_readback_buffers[i] =
                stats_readback_buffers_[i].Get();

        pass_stats_.init(device_.Get(), program_argument_, resources);
    }

    void RendererRasterStats::render_prepare_() {
        this->collect_completed_stats_(frame_index_);
        RendererBenchmark::render_prepare_();
        this->build_visible_draws_();
    }

    void RendererRasterStats::render_record_() {
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_stats_.render(
            command_list_.Get(),
            frame_index_,
            visible_draws_,
            width_,
            height_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);

        pending_stats_[frame_index_] = {
            true,
            frame_number_,
            visible_triangle_count_
        };
        ++frame_number_;
    }

    void RendererRasterStats::before_close_() {
        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i)
            this->collect_completed_stats_(i);
        this->write_stats_csv_();
    }

    void RendererRasterStats::allocate_counter_buffers_() {
        const std::uint64_t pixel_count64 =
            static_cast<std::uint64_t>(width_) * static_cast<std::uint64_t>(height_);
        util::Logger::g_logger.assert_with_log(
            pixel_count64 > 0 &&
            pixel_count64 <= std::numeric_limits<std::uint32_t>::max(),
            "raster stats pixel count must fit in uint32");

        pixel_count_ = static_cast<std::uint32_t>(pixel_count64);

        const std::uint64_t pixel_buffer_size =
            pixel_count64 * sizeof(std::uint32_t);
        const std::uint64_t stats_buffer_size =
            PassRasterStats::STATS_COUNTER_COUNT * sizeof(std::uint32_t);

        pixel_count_buffer_.init(
            create_uav_buffer(device_.Get(), pixel_buffer_size).Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        stats_buffer_.init(
            create_uav_buffer(device_.Get(), stats_buffer_size).Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
            stats_readback_buffers_[i] = dxutl::create_buffer(
                device_.Get(),
                stats_buffer_size,
                D3D12_HEAP_TYPE_READBACK,
                D3D12_RESOURCE_STATE_COPY_DEST);
        }
    }

    void RendererRasterStats::allocate_draw_buffers_() {
        const std::uint64_t max_draws =
            this->count_draw_chunks_(scene_cpu_->draw_calls);

        util::Logger::g_logger.assert_with_log(
            max_draws <= std::numeric_limits<std::uint32_t>::max(),
            "raster stats draw count must fit in uint32");
        util::Logger::g_logger.assert_with_log_mul_overflow(
            max_draws,
            sizeof(RasterStatsDraw),
            std::numeric_limits<std::uint64_t>::max(),
            "raster stats draw buffer size overflow");

        draw_capacity_ = static_cast<std::uint32_t>(
            (std::max<std::uint64_t>)(max_draws, 1));
        const std::uint64_t draw_buffer_size =
            static_cast<std::uint64_t>(draw_capacity_) *
            sizeof(RasterStatsDraw);

        draw_buffer_.init(
            dxutl::create_buffer(
                device_.Get(),
                draw_buffer_size,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COPY_DEST).Get(),
            D3D12_RESOURCE_STATE_COPY_DEST);
        draw_upload_buffer_ = dxutl::create_buffer(
            device_.Get(),
            draw_buffer_size,
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_RESOURCE_STATE_GENERIC_READ);

        visible_draws_.reserve(draw_capacity_);
    }

    std::uint64_t RendererRasterStats::count_draw_chunks_(
        const std::vector<scene::SceneCPUData::DrawCall>& draws) const {

        std::uint64_t result = 0;
        for (const scene::SceneCPUData::DrawCall& draw : draws) {
            const std::uint32_t triangles_per_instance =
                draw.index_count / 3u;
            if (triangles_per_instance == 0 || draw.instance_count == 0)
                continue;

            const std::uint32_t max_instances_per_chunk =
                (std::max<std::uint32_t>)(
                    1u,
                    std::numeric_limits<std::uint32_t>::max() /
                    triangles_per_instance);
            const std::uint64_t chunk_count =
                (static_cast<std::uint64_t>(draw.instance_count) +
                    max_instances_per_chunk - 1u) /
                max_instances_per_chunk;
            util::Logger::g_logger.assert_with_log(
                result <= std::numeric_limits<std::uint64_t>::max() -
                chunk_count,
                "raster stats draw chunk count overflow");
            result += chunk_count;
        }
        return result;
    }

    std::uint64_t RendererRasterStats::count_draw_triangles_(
        const std::vector<scene::SceneCPUData::DrawCall>& draws) const {

        std::uint64_t result = 0;
        for (const scene::SceneCPUData::DrawCall& draw : draws) {
            const std::uint64_t draw_triangle_count =
                (static_cast<std::uint64_t>(draw.index_count) / 3u) *
                draw.instance_count;
            util::Logger::g_logger.assert_with_log(
                result <= std::numeric_limits<std::uint64_t>::max() -
                draw_triangle_count,
                "raster stats triangle count overflow");
            result += draw_triangle_count;
        }
        return result;
    }

    void RendererRasterStats::build_visible_draws_() {
        visible_draws_.clear();

        const std::uint64_t visible_draw_count =
            this->count_draw_chunks_(draw_stream_.draw_calls_compacted);
        visible_triangle_count_ =
            this->count_draw_triangles_(draw_stream_.draw_calls_compacted);
        util::Logger::g_logger.assert_with_log(
            visible_draw_count <= draw_capacity_,
            "visible draw count exceeds raster stats buffer capacity");

        visible_draws_.reserve(static_cast<std::size_t>(visible_draw_count));

        for (const auto& draw : draw_stream_.draw_calls_compacted) {
            util::Logger::g_logger.assert_with_log(
                draw.submesh_id < scene_cpu_->submeshes.size() &&
                static_cast<std::uint64_t>(draw.first_instance) +
                draw.instance_count <=
                draw_stream_.draw_instance_ids_compacted.size(),
                "raster stats draw stream has an invalid draw");

            const std::uint32_t triangles_per_instance =
                draw.index_count / 3u;
            if (triangles_per_instance == 0 || draw.instance_count == 0)
                continue;

            const std::uint32_t max_instances_per_chunk =
                (std::max<std::uint32_t>)(
                    1u,
                    std::numeric_limits<std::uint32_t>::max() /
                    triangles_per_instance);
            std::uint32_t first_draw_instance = draw.first_instance;
            std::uint32_t remaining_instances = draw.instance_count;
            while (remaining_instances > 0) {
                const std::uint32_t chunk_instances =
                    (std::min<std::uint32_t>)(
                        remaining_instances,
                        max_instances_per_chunk);
                visible_draws_.push_back({
                    first_draw_instance,
                    chunk_instances,
                    draw.index_offset,
                    draw.index_count,
                    draw.vertex_offset
                });
                first_draw_instance += chunk_instances;
                remaining_instances -= chunk_instances;
            }
        }

        if (!visible_draws_.empty()) {
            const std::uint64_t bytes =
                static_cast<std::uint64_t>(visible_draws_.size()) *
                sizeof(RasterStatsDraw);
            dxutl::copy_to_upload_buffer(
                draw_upload_buffer_.Get(),
                visible_draws_.data(),
                static_cast<std::size_t>(bytes));
        }
    }

    void RendererRasterStats::collect_completed_stats_(UINT frame_index) {
        PendingStatsFrame& pending = pending_stats_[frame_index];
        if (!pending.valid)
            return;

        const std::uint64_t measure_begin = program_argument_.warmup_frames;
        const std::uint64_t measure_end =
            measure_begin + program_argument_.measure_frames;

        if (measure_begin <= pending.frame_number &&
            pending.frame_number < measure_end) {

            StatsRow row{};
            row.frame = pending.frame_number;
            row.triangle_count = pending.triangle_count;

            D3D12_RANGE read_range{
                0,
                PassRasterStats::STATS_COUNTER_COUNT * sizeof(std::uint32_t)
            };
            void* mapped = nullptr;
            util::Utils::throw_if_failed(
                stats_readback_buffers_[frame_index]->Map(
                    0, &read_range, &mapped),
                "map raster stats readback buffer");
            const auto* counters = static_cast<const std::uint32_t*>(mapped);
            std::copy_n(
                counters,
                PassRasterStats::STATS_COUNTER_COUNT,
                row.counters.begin());
            D3D12_RANGE written_range{ 0, 0 };
            stats_readback_buffers_[frame_index]->Unmap(0, &written_range);

            stats_rows_.push_back(row);
        }

        pending.valid = false;
    }

    void RendererRasterStats::write_stats_csv_() const {
        if (program_argument_.output_filepath.empty())
            return;

        const std::filesystem::path path =
            make_stats_output_path(program_argument_);
        std::ofstream output(path, std::ios::out | std::ios::trunc);
        if (!output) {
            util::Logger::g_logger
                << "Failed to open raster stats CSV: "
                << path.string() << '\n';
            return;
        }

        output
            << "frame,triangle_count,total_fragments,covered_pixels,"
            << "overdraw_extra,avg_overdraw,max_overdraw,"
            << "rasterized_triangles,skipped_triangles,"
            << "quad_instances,quad_covered_lanes,quad_waste_lanes,"
            << "quad_efficiency\n";
        output << std::fixed << std::setprecision(6);

        for (const StatsRow& row : stats_rows_) {
            const auto& c = row.counters;
            const double avg_overdraw = safe_ratio(
                c[TOTAL_FRAGMENTS], c[COVERED_PIXELS]);
            const double quad_efficiency = safe_ratio(
                c[QUAD_COVERED_LANES],
                c[QUAD_COVERED_LANES] + c[QUAD_WASTE_LANES]);

            output
                << row.frame << ','
                << row.triangle_count << ','
                << c[TOTAL_FRAGMENTS] << ','
                << c[COVERED_PIXELS] << ','
                << c[OVERDRAW_EXTRA] << ','
                << avg_overdraw << ','
                << c[MAX_OVERDRAW] << ','
                << c[RASTERIZED_TRIANGLES] << ','
                << c[SKIPPED_TRIANGLES] << ','
                << c[QUAD_INSTANCES] << ','
                << c[QUAD_COVERED_LANES] << ','
                << c[QUAD_WASTE_LANES] << ','
                << quad_efficiency << '\n';
        }

        std::cout
            << "Saved raster stats CSV: " << path.string() << '\n';
    }

}
