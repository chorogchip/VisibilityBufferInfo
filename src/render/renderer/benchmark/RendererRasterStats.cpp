#include "render/renderer/benchmark/RendererRasterStats.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
        this->allocate_triangle_buffers_();

        PassRasterStatsResources resources{};
        resources.constant_buffer_addresses[0] =
            buf_constant_[0].get()->GetGPUVirtualAddress();
        resources.constant_buffer_addresses[1] =
            buf_constant_[1].get()->GetGPUVirtualAddress();
        static_assert(util::Constants::FRAME_COUNT == 2);
        resources.triangle_buffer = &triangle_buffer_;
        resources.triangle_upload_buffer = triangle_upload_buffer_.Get();
        resources.vertex_buffer = &scene_vertex_buffer_;
        resources.object_buffer = &scene_object_buffer_;
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
        this->build_visible_triangles_();
    }

    void RendererRasterStats::render_record_() {
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);
        pass_stats_.render(
            command_list_.Get(),
            frame_index_,
            static_cast<std::uint32_t>(visible_triangles_.size()),
            width_,
            height_);
        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);

        pending_stats_[frame_index_] = {
            true,
            frame_number_,
            static_cast<std::uint32_t>(visible_triangles_.size())
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

    void RendererRasterStats::allocate_triangle_buffers_() {
        const auto& max_batches = scene_cpu_->all_batches.empty()
            ? scene_cpu_->batches
            : scene_cpu_->all_batches;
        const std::uint64_t max_triangles =
            this->count_batch_triangles_(max_batches);

        util::Logger::g_logger.assert_with_log(
            max_triangles <= std::numeric_limits<std::uint32_t>::max(),
            "raster stats triangle count must fit in uint32");
        util::Logger::g_logger.assert_with_log_mul_overflow(
            max_triangles,
            sizeof(RasterStatsTriangle),
            std::numeric_limits<std::uint64_t>::max(),
            "raster stats triangle buffer size overflow");

        triangle_capacity_ = static_cast<std::uint32_t>(
            (std::max<std::uint64_t>)(max_triangles, 1));
        const std::uint64_t triangle_buffer_size =
            static_cast<std::uint64_t>(triangle_capacity_) *
            sizeof(RasterStatsTriangle);

        triangle_buffer_.init(
            dxutl::create_buffer(
                device_.Get(),
                triangle_buffer_size,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COPY_DEST).Get(),
            D3D12_RESOURCE_STATE_COPY_DEST);
        triangle_upload_buffer_ = dxutl::create_buffer(
            device_.Get(),
            triangle_buffer_size,
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_RESOURCE_STATE_GENERIC_READ);

        visible_triangles_.reserve(triangle_capacity_);
    }

    std::uint64_t RendererRasterStats::count_batch_triangles_(
        const std::vector<scene::SceneDataCPU::ObjectBatch>& batches) const {

        std::uint64_t result = 0;
        for (const auto& batch : batches) {
            util::Logger::g_logger.assert_with_log(
                batch.mesh_index < scene_cpu_->meshes.size(),
                "raster stats batch has an invalid mesh index");
            const auto& mesh = scene_cpu_->meshes[batch.mesh_index];
            result += (static_cast<std::uint64_t>(mesh.index_count) / 3u) *
                static_cast<std::uint64_t>(batch.object_count);
        }
        return result;
    }

    void RendererRasterStats::build_visible_triangles_() {
        visible_triangles_.clear();

        const std::uint64_t visible_count =
            this->count_batch_triangles_(scene_cpu_->batches);
        util::Logger::g_logger.assert_with_log(
            visible_count <= triangle_capacity_,
            "visible triangle count exceeds raster stats buffer capacity");

        visible_triangles_.reserve(static_cast<std::size_t>(visible_count));

        for (const auto& batch : scene_cpu_->batches) {
            util::Logger::g_logger.assert_with_log(
                batch.mesh_index < scene_cpu_->meshes.size(),
                "raster stats batch has an invalid mesh index");

            const auto& mesh = scene_cpu_->meshes[batch.mesh_index];
            for (std::uint32_t instance = 0; instance < batch.object_count; ++instance) {
                const std::uint32_t object_index = batch.object_index + instance;
                util::Logger::g_logger.assert_with_log(
                    object_index < scene_cpu_->objects.size(),
                    "raster stats batch has an invalid object range");

                for (std::uint32_t local = 0; local + 2 < mesh.index_count; local += 3) {
                    const std::uint32_t index_base = mesh.index_start + local;
                    visible_triangles_.push_back({
                        object_index,
                        scene_cpu_->indices[index_base + 0] + mesh.vertex_start,
                        scene_cpu_->indices[index_base + 1] + mesh.vertex_start,
                        scene_cpu_->indices[index_base + 2] + mesh.vertex_start,
                    });
                }
            }
        }

        if (!visible_triangles_.empty()) {
            const std::uint64_t bytes =
                static_cast<std::uint64_t>(visible_triangles_.size()) *
                sizeof(RasterStatsTriangle);
            dxutl::copy_to_upload_buffer(
                triangle_upload_buffer_.Get(),
                visible_triangles_.data(),
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

        util::Logger::g_logger
            << "Saved raster stats CSV: " << path.string() << '\n';
    }

}
