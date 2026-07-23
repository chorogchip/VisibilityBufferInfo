#pragma once

#include <cstdint>

#include <d3d12.h>

#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include "util/Constants.h"

namespace eng {
    class GPUResource;
}

namespace rndr {

    struct PassRasterStatsResources {
        D3D12_GPU_VIRTUAL_ADDRESS constant_buffer_addresses[util::Constants::FRAME_COUNT]{};
        eng::GPUResource* triangle_buffer = nullptr;
        ID3D12Resource* triangle_upload_buffer = nullptr;
        eng::GPUResource* vertex_buffer = nullptr;
        eng::GPUResource* object_buffer = nullptr;
        eng::GPUResource* pixel_count_buffer = nullptr;
        eng::GPUResource* stats_buffer = nullptr;
        ID3D12Resource* stats_readback_buffers[util::Constants::FRAME_COUNT]{};
    };

    class PassRasterStats {
    public:
        static constexpr std::uint32_t TRIANGLE_STRIDE_BYTES = 16;
        static constexpr std::uint32_t STATS_COUNTER_COUNT = 16;

        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            const PassRasterStatsResources& resources);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            std::uint32_t triangle_count,
            std::uint32_t width,
            std::uint32_t height);

    private:
        PassRasterStatsResources resources_{};
        eng::GraphicsPipeline clear_pso_;
        eng::GraphicsPipeline count_pso_;
        eng::GraphicsPipeline reduce_pso_;
    };

}
