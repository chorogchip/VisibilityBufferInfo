#include "render/pass/benchmark/PassRasterStats.h"

#include <algorithm>
#include <limits>

#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/RootSignatureBuilder.h"
#include "util/Logger.h"
#include "util/minmax_remover.h"

namespace rndr {

    namespace {

        enum class RootParam : UINT {
            FRAME_CONSTANT,
            DISPATCH_CONSTANT,
            DRAW_BUFFER,
            INDEX_BUFFER,
            VERTEX_BUFFER,
            INSTANCE_BUFFER,
            DRAW_INSTANCE_BUFFER,
            DRAW_INSTANCE_ID_BUFFER,
            PIXEL_COUNT_BUFFER,
            STATS_BUFFER,
        };

        struct DispatchConstants {
            std::uint32_t draw_index = 0;
            std::uint32_t triangle_start = 0;
            std::uint32_t triangle_count = 0;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t pixel_count = 0;
            std::uint32_t count_group_count_x = 1;
            std::uint32_t padding0 = 0;
        };

        constexpr UINT CLEAR_REDUCE_THREAD_WIDTH = 16;
        constexpr UINT COUNT_THREAD_COUNT = 64;
        constexpr UINT MAX_DISPATCH_GROUPS_PER_DIMENSION = 65535;

        struct DispatchGroupCount {
            UINT x = 1;
            UINT y = 1;
        };

        DispatchGroupCount get_count_dispatch_groups(
            std::uint32_t triangle_count) {
            const std::uint64_t group_count =
                (static_cast<std::uint64_t>(triangle_count) +
                    COUNT_THREAD_COUNT - 1u) /
                COUNT_THREAD_COUNT;
            const UINT x = group_count == 0
                ? 1
                : (std::min<std::uint64_t>)(
                    group_count,
                    MAX_DISPATCH_GROUPS_PER_DIMENSION);
            const UINT y = group_count == 0
                ? 1
                : static_cast<UINT>((group_count + x - 1u) / x);

            util::Logger::g_logger.assert_with_log(
                y <= MAX_DISPATCH_GROUPS_PER_DIMENSION,
                "raster stats dispatch exceeds D3D12 group dimension limits");

            return { x, y };
        }

        void bind_common_roots(
            ID3D12GraphicsCommandList* command_list,
            ID3D12RootSignature* root_signature,
            const PassRasterStatsResources& resources,
            UINT frame_index,
            const DispatchConstants& constants) {

            command_list->SetComputeRootSignature(root_signature);
            command_list->SetComputeRootConstantBufferView(
                static_cast<UINT>(RootParam::FRAME_CONSTANT),
                resources.constant_buffer_addresses[frame_index]);
            command_list->SetComputeRoot32BitConstants(
                static_cast<UINT>(RootParam::DISPATCH_CONSTANT),
                sizeof(DispatchConstants) / sizeof(std::uint32_t),
                &constants,
                0);
            command_list->SetComputeRootShaderResourceView(
                static_cast<UINT>(RootParam::DRAW_BUFFER),
                resources.draw_buffer->get()->GetGPUVirtualAddress());
            command_list->SetComputeRootShaderResourceView(
                static_cast<UINT>(RootParam::INDEX_BUFFER),
                resources.index_buffer->get()->GetGPUVirtualAddress());
            command_list->SetComputeRootShaderResourceView(
                static_cast<UINT>(RootParam::VERTEX_BUFFER),
                resources.vertex_buffer->get()->GetGPUVirtualAddress());
            command_list->SetComputeRootShaderResourceView(
                static_cast<UINT>(RootParam::INSTANCE_BUFFER),
                resources.instance_buffer->get()->GetGPUVirtualAddress());
            command_list->SetComputeRootShaderResourceView(
                static_cast<UINT>(RootParam::DRAW_INSTANCE_BUFFER),
                resources.draw_instance_buffer->get()->GetGPUVirtualAddress());
            command_list->SetComputeRootShaderResourceView(
                static_cast<UINT>(RootParam::DRAW_INSTANCE_ID_BUFFER),
                resources.draw_instance_id_buffer->get()->GetGPUVirtualAddress());
            command_list->SetComputeRootUnorderedAccessView(
                static_cast<UINT>(RootParam::PIXEL_COUNT_BUFFER),
                resources.pixel_count_buffer->get()->GetGPUVirtualAddress());
            command_list->SetComputeRootUnorderedAccessView(
                static_cast<UINT>(RootParam::STATS_BUFFER),
                resources.stats_buffer->get()->GetGPUVirtualAddress());
        }
    }

    void PassRasterStats::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassRasterStatsResources& resources) {

        resources_ = resources;

        auto cs_clear = dxutl::compile_shader(
            L"assets/shaders/benchmark_raster_stats_CS.hlsl",
            "cs_5_0", "clear_main", arguments);
        auto cs_count = dxutl::compile_shader(
            L"assets/shaders/benchmark_raster_stats_CS.hlsl",
            "cs_5_0", "count_main", arguments);
        auto cs_reduce = dxutl::compile_shader(
            L"assets/shaders/benchmark_raster_stats_CS.hlsl",
            "cs_5_0", "reduce_main", arguments);

        auto root_signature = eng::RootSignatureBuilder{}
            .root_cbv().reg(0).add()
            .constant().reg(1).cnt(8).add()
            .root_srv().reg(0).add()
            .root_srv().reg(1).add()
            .root_srv().reg(2).add()
            .root_srv().reg(3).add()
            .root_srv().reg(4).add()
            .root_srv().reg(5).add()
            .root_uav().reg(0).add()
            .root_uav().reg(1).add()
            .build(device);

        clear_pso_.init(device);
        clear_pso_.set_compute();
        clear_pso_.set_root_signature(root_signature.Get());
        clear_pso_.set_shader_compute(cs_clear.Get());
        clear_pso_.build();

        count_pso_.init(device);
        count_pso_.set_compute();
        count_pso_.set_root_signature(root_signature.Get());
        count_pso_.set_shader_compute(cs_count.Get());
        count_pso_.build();

        reduce_pso_.init(device);
        reduce_pso_.set_compute();
        reduce_pso_.set_root_signature(root_signature.Get());
        reduce_pso_.set_shader_compute(cs_reduce.Get());
        reduce_pso_.build();
    }

    void PassRasterStats::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        std::span<const RasterStatsDraw> draws,
        std::uint32_t width,
        std::uint32_t height) {

        util::Logger::g_logger.assert_with_log(
            width > 0 && height > 0,
            "raster stats pass requires a non-empty viewport");

        const std::uint64_t pixel_count64 =
            static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
        util::Logger::g_logger.assert_with_log(
            pixel_count64 <= std::numeric_limits<std::uint32_t>::max(),
            "raster stats pixel count exceeds uint32 range");

        util::Logger::g_logger.assert_with_log(
            draws.size() <= std::numeric_limits<std::uint32_t>::max(),
            "raster stats draw count exceeds uint32 range");

        if (!draws.empty()) {
            const std::uint64_t draw_bytes =
                static_cast<std::uint64_t>(draws.size()) * DRAW_STRIDE_BYTES;
            resources_.draw_buffer->transition(command_list, D3D12_RESOURCE_STATE_COPY_DEST);
            command_list->CopyBufferRegion(
                resources_.draw_buffer->get(),
                0,
                resources_.draw_upload_buffer,
                0,
                draw_bytes);
        }

        resources_.draw_buffer->transition(
            command_list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        resources_.index_buffer->transition(
            command_list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        resources_.vertex_buffer->transition(
            command_list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        resources_.instance_buffer->transition(
            command_list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        resources_.draw_instance_buffer->transition(
            command_list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        resources_.draw_instance_id_buffer->transition(
            command_list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        resources_.pixel_count_buffer->transition(
            command_list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        resources_.stats_buffer->transition(
            command_list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        const std::uint32_t pixel_count = static_cast<std::uint32_t>(pixel_count64);
        const UINT clear_reduce_group_x =
            (width + CLEAR_REDUCE_THREAD_WIDTH - 1) / CLEAR_REDUCE_THREAD_WIDTH;
        const UINT clear_reduce_group_y =
            (height + CLEAR_REDUCE_THREAD_WIDTH - 1) / CLEAR_REDUCE_THREAD_WIDTH;

        util::Logger::g_logger.assert_with_log(
            clear_reduce_group_x <= MAX_DISPATCH_GROUPS_PER_DIMENSION &&
            clear_reduce_group_y <= MAX_DISPATCH_GROUPS_PER_DIMENSION,
            "raster stats dispatch exceeds D3D12 group dimension limits");

        DispatchConstants constants{};
        constants.width = width;
        constants.height = height;
        constants.pixel_count = pixel_count;

        command_list->SetPipelineState(clear_pso_.get());
        bind_common_roots(
            command_list, clear_pso_.get_root_signature(),
            resources_, frame_index, constants);
        command_list->Dispatch(clear_reduce_group_x, clear_reduce_group_y, 1);

        resources_.pixel_count_buffer->uav_barrier(command_list);
        resources_.stats_buffer->uav_barrier(command_list);

        if (!draws.empty()) {
            command_list->SetPipelineState(count_pso_.get());
            for (std::uint32_t draw_index = 0;
                draw_index < static_cast<std::uint32_t>(draws.size());
                ++draw_index) {
                const RasterStatsDraw& draw = draws[draw_index];
                const std::uint32_t triangles_per_instance =
                    draw.index_count / 3u;
                const std::uint64_t triangle_count64 =
                    static_cast<std::uint64_t>(triangles_per_instance) *
                    draw.instance_count;
                if (triangle_count64 == 0)
                    continue;
                util::Logger::g_logger.assert_with_log(
                    triangle_count64 <=
                    std::numeric_limits<std::uint32_t>::max(),
                    "raster stats draw triangle count exceeds uint32 range");

                const std::uint32_t draw_triangle_count =
                    static_cast<std::uint32_t>(triangle_count64);
                const DispatchGroupCount count_groups =
                    get_count_dispatch_groups(draw_triangle_count);

                constants.draw_index = draw_index;
                constants.triangle_start = 0;
                constants.triangle_count = draw_triangle_count;
                constants.count_group_count_x = count_groups.x;
                bind_common_roots(
                    command_list, count_pso_.get_root_signature(),
                    resources_, frame_index, constants);
                command_list->Dispatch(count_groups.x, count_groups.y, 1);

                resources_.pixel_count_buffer->uav_barrier(command_list);
                resources_.stats_buffer->uav_barrier(command_list);
            }
        }

        command_list->SetPipelineState(reduce_pso_.get());
        bind_common_roots(
            command_list, reduce_pso_.get_root_signature(),
            resources_, frame_index, constants);
        command_list->Dispatch(clear_reduce_group_x, clear_reduce_group_y, 1);

        resources_.stats_buffer->uav_barrier(command_list);
        resources_.stats_buffer->transition(command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
        command_list->CopyBufferRegion(
            resources_.stats_readback_buffers[frame_index],
            0,
            resources_.stats_buffer->get(),
            0,
            STATS_COUNTER_COUNT * sizeof(std::uint32_t));
    }

}
