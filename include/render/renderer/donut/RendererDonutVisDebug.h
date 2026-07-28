#pragma once

#include <array>

#include "dx_util/UploadConstBuf.h"
#include "engine/GPUResource.h"
#include "render/pass/donut/PassDonutVisDebugResolve.h"
#include "render/pass/donut/PassDonutVisibility.h"
#include "render/renderer/VisibilityDebugMode.h"
#include "render/renderer/donut/RendererDonut.h"
#include "scene/donut/DonutRenderConstants.h"

namespace rndr {

    class RendererDonutVisDebug : public RendererDonut {
    public:
        explicit RendererDonutVisDebug(VisibilityDebugMode mode)
            : mode_(mode) {}
        ~RendererDonutVisDebug() override = default;

    private:
        void init2_() override;
        void render_prepare_donut_() override;
        void render_record_() override;

        VisibilityDebugMode mode_;
        PassDonutVisibility pass_visibility_;
        PassDonutVisDebugResolve pass_debug_resolve_;
        eng::GPUResource visibility_buffer_;
        std::array<dxutl::UploadConstBuf<scene::DonutGBufferFillConstants>,
            util::Constants::FRAME_COUNT> gbuffer_constants_;
        std::array<eng::GPUResource, util::Constants::FRAME_COUNT>
            gbuffer_constant_resources_;
        scene::DonutPlanarViewConstants previous_view_constants_{};
        bool has_previous_view_constants_ = false;
    };

}
