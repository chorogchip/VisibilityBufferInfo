#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "dx_util/UploadConstBuf.h"
#include "engine/GPUResource.h"
#include "render/pass/donut/PassDonutDeferredLighting.h"
#include "render/pass/donut/PassDonutTonemap.h"
#include "render/pass/donut/PassDonutVisibility.h"
#include "render/pass/donut/PassDonutVisUtil.h"
#include "render/pass/donut/PassDonutVisGbuffer.h"
#include "render/renderer/donut/DonutCompactDebugMode.h"
#include "render/renderer/donut/DonutNeutralResources.h"
#include "render/renderer/donut/RendererDonut.h"
#include "scene/donut/DonutRenderConstants.h"

namespace rndr {

    class RendererDonutVisGBuffer : public RendererDonut {
    public:
        RendererDonutVisGBuffer() = default;
        explicit RendererDonutVisGBuffer(
            DonutCompactDebugMode compact_debug_mode)
            : compact_debug_mode_(compact_debug_mode) {}
        ~RendererDonutVisGBuffer() override = default;

    private:
        void init2_() override;
        void render_prepare_donut_() override;
        void render_record_() override;

        static constexpr UINT GBUFFER_COUNT = 4;

        PassDonutVisibility pass_visibility_;
        PassDonutVisUtil pass_visutil_;
        PassDonutVisGBuffer pass_gbuffer_;
        PassDonutDeferredLighting pass_lighting_;
        PassDonutTonemap pass_tonemap_;

        DonutNeutralResources neutral_resources_;

        eng::GPUResource visibility_buffer_;
        eng::GPUResource visutil_pixel_list_;
        eng::GPUResource indirect_dispatch_list_;
        std::array<eng::GPUResource, GBUFFER_COUNT> gbuffers_;
        eng::GPUResource exposure_buffer_;
        eng::GPUResource hdr_color_buffer_;

        std::array<dxutl::UploadConstBuf<scene::DonutGBufferFillConstants>,
            util::Constants::FRAME_COUNT> gbuffer_constants_;
        std::array<dxutl::UploadConstBuf<scene::DonutDeferredLightingConstants>,
            util::Constants::FRAME_COUNT> lighting_constants_;
        std::array<eng::GPUResource, util::Constants::FRAME_COUNT>
            gbuffer_constant_resources_;
        std::array<eng::GPUResource, util::Constants::FRAME_COUNT>
            lighting_constant_resources_;

        scene::DonutPlanarViewConstants previous_view_constants_{};
        bool has_previous_view_constants_ = false;
        std::uint32_t donut_frame_index_ = 0;
        std::optional<DonutCompactDebugMode> compact_debug_mode_;
    };
}
