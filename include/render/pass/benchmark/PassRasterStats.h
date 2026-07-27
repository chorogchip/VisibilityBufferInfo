#pragma once

#include <cstdint>
#include <span>

#include <d3d12.h>

#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include "util/Constants.h"

namespace eng {
    class GPUResource;
}

namespace rndr {

    struct RasterStatsDraw {
        std::uint32_t first_draw_instance = 0;
        std::uint32_t instance_count = 0;
        std::uint32_t index_offset = 0;
        std::uint32_t index_count = 0;
        std::uint32_t vertex_offset = 0;
    };
    static_assert(sizeof(RasterStatsDraw) == 20);

    struct PassRasterStatsResources {
        D3D12_GPU_VIRTUAL_ADDRESS constant_buffer_addresses[util::Constants::FRAME_COUNT]{};
        eng::GPUResource* draw_buffer = nullptr;
        ID3D12Resource* draw_upload_buffer = nullptr;
        eng::GPUResource* index_buffer = nullptr;
        eng::GPUResource* vertex_buffer = nullptr;
        eng::GPUResource* instance_buffer = nullptr;
        eng::GPUResource* draw_instance_buffer = nullptr;
        eng::GPUResource* draw_instance_id_buffer = nullptr;
        eng::GPUResource* pixel_count_buffer = nullptr;
        eng::GPUResource* stats_buffer = nullptr;
        ID3D12Resource* stats_readback_buffers[util::Constants::FRAME_COUNT]{};
    };

    class PassRasterStats {
    public:
        static constexpr std::uint32_t DRAW_STRIDE_BYTES =
            sizeof(RasterStatsDraw);
        static constexpr std::uint32_t STATS_COUNTER_COUNT = 16;

        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            const PassRasterStatsResources& resources);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            std::span<const RasterStatsDraw> draws,
            std::uint32_t width,
            std::uint32_t height);

    private:
        PassRasterStatsResources resources_{};
        eng::GraphicsPipeline clear_pso_;
        eng::GraphicsPipeline count_pso_;
        eng::GraphicsPipeline reduce_pso_;
    };

}
