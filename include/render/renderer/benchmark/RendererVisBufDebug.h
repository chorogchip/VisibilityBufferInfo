#pragma once

#include "engine/GPUResource.h"
#include "render/pass/benchmark/PassVisibility.h"
#include "render/pass/benchmark/PassVisBufDebugResolve.h"
#include "render/renderer/VisibilityDebugMode.h"
#include "render/renderer/benchmark/RendererBenchmark.h"

namespace rndr {

    class RendererVisBufDebug : public RendererBenchmark {
    public:
        explicit RendererVisBufDebug(VisibilityDebugMode mode)
            : mode_(mode) {}
        ~RendererVisBufDebug() override = default;

    private:
        void init2_() override;
        void render_record_() override;

        VisibilityDebugMode mode_;
        eng::GPUResource visibility_buffer_;
        PassVisibility pass_visibility_;
        PassVisBufDebugResolve pass_debug_resolve_;
    };

}
