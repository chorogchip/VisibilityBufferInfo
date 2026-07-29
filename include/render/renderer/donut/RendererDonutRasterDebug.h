#pragma once

#include <array>

#include "dx_util/UploadConstBuf.h"
#include "engine/GPUResource.h"
#include "render/pass/donut/PassDonutRasterDebug.h"
#include "render/renderer/VisibilityDebugMode.h"
#include "render/renderer/donut/RendererDonut.h"
#include "scene/donut/DonutRenderConstants.h"

namespace rndr {

    class RendererDonutRasterDebug : public RendererDonut {
    public:
        explicit RendererDonutRasterDebug(VisibilityDebugMode mode)
            : mode_(mode) {}
        ~RendererDonutRasterDebug() override = default;

    private:
        void init2_() override;
        void render_prepare_donut_() override;
        void render_record_() override;

        VisibilityDebugMode mode_;
        PassDonutRasterDebug pass_debug_;
        std::array<dxutl::UploadConstBuf<scene::DonutGBufferFillConstants>,
            util::Constants::FRAME_COUNT> gbuffer_constants_;
        std::array<eng::GPUResource, util::Constants::FRAME_COUNT>
            gbuffer_constant_resources_;
        scene::DonutPlanarViewConstants previous_view_constants_{};
        bool has_previous_view_constants_ = false;
    };

}
