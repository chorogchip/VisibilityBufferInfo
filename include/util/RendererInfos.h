#pragma once

namespace util {

    enum class EnumRenderVariants {
        FORWARD,
        FORWARD_PREPASS,
        DEFERRED,
        DEFERRED_PREPASS,
        VISBUF_BASIC,
        VISBUF_BASIC_GBUFFER,
    };

    class RenderVariant {
    public:
        EnumRenderVariants variant;

        constexpr RenderVariant(EnumRenderVariants variant_) : variant{ variant_ } {}
        constexpr operator EnumRenderVariants() const noexcept { return variant; }

        bool do_use_gbuffer() {
            return
                variant == EnumRenderVariants::DEFERRED ||
                variant == EnumRenderVariants::DEFERRED_PREPASS ||
                variant == EnumRenderVariants::VISBUF_BASIC_GBUFFER;
        }
        bool is_visbuf() {
            return
                variant == EnumRenderVariants::VISBUF_BASIC ||
                variant == EnumRenderVariants::VISBUF_BASIC_GBUFFER;
        }
    };

}