#pragma once

#include <vector>

namespace rndr {
    struct RenderVariant {
        enum class EnumRenderAlgorithm {
            FORWARD,
            FORWARD_PREPASS,
            DEFERRED,
            DEFERRED_PREPASS,
            VISBUF_BASIC,
            VISBUF_BASIC_GBUFFER,
        };

    public:
        EnumRenderAlgorithm algorithm;
        constexpr RenderVariant() = default;
    };

    struct RenderVariantCompiled {

        struct RenderPass {
            enum class EnumPassT {
                FORWARD,
                GBUFFER,
                DEPTH_PREPASS,
                DEFERRED_SHADING,
                VISIBILITY,
                VISBUF_TO_GBUFFER,
            };
        };

        struct RenderResourceRequire {

        };

        struct RenderBindingLayout {

        };

        struct RootSignatureLayout {

        };

        struct ShaderPermutation {

        };

        struct PipelineFormats {

        };


        RenderVariant source;

        std::vector<RenderPass> passes;
        std::vector<RenderResourceRequire> resources;

        RenderBindingLayout binding_layout;
        RootSignatureLayout root_sig_layout;

        ShaderPermutation shaders;
        PipelineFormats formats;
    };
}