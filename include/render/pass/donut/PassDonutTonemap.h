#pragma once

#include <array>
#include <DirectXMath.h>

#include "ProgramArgument.h"
#include "util/Constants.h"
#include "dx_util/UploadConstBuf.h"
#include "engine/GPUResource.h"
#include "engine/GraphicsPipeline.h"

namespace eng {
    class GPUResource;
    class ResourceManagerFrame;
    class ResourceManagerShader;
    class ResourceManagerSampler;
}

namespace rndr {

    struct PassDonutTonemapResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::ResourceManagerSampler* sampler_manager = nullptr;
        eng::GPUResource* back_buffers[util::Constants::FRAME_COUNT]{};
        eng::GPUResource* hdr_color_buffer = nullptr;
        eng::GPUResource* exposure_buffer = nullptr;
    };

    class PassDonutTonemap {

    public:
        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            const PassDonutTonemapResources& resources);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            const D3D12_VIEWPORT& viewport,
            const D3D12_RECT& scissor_rect);

        struct DonutToneMappingConstants {
            std::uint32_t view_origin[2]{};
            std::uint32_t view_size[2]{};
            float log_luminance_scale = 0.0f;
            float log_luminance_bias = 0.0f;
            float histogram_low_percentile = 0.0f;
            float histogram_high_percentile = 0.0f;
            float eye_adaptation_speed_up = 0.0f;
            float eye_adaptation_speed_down = 0.0f;
            float min_adapted_luminance = 1.0f;
            float max_adapted_luminance = 1.0f;
            float frame_time = 0.0f;
            float exposure_scale = 1.0f;
            float white_point_inv_squared = 1.0f;
            std::uint32_t source_slice = 0;
            DirectX::XMFLOAT2 color_lut_texture_size{};
            DirectX::XMFLOAT2 color_lut_texture_size_inv{};
        };
        static_assert(sizeof(DonutToneMappingConstants) == 80);

    private:
        PassDonutTonemapResources resources_{};
        eng::GraphicsPipeline pso_;

        std::array<dxutl::UploadConstBuf<DonutToneMappingConstants>,
            util::Constants::FRAME_COUNT> tonemap_constants_;
        std::array<eng::GPUResource, util::Constants::FRAME_COUNT>
            tonemap_constant_resources_;
        eng::GPUResource fallback_color_lut_;
    };
}
