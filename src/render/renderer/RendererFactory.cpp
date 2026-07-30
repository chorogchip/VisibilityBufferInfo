#include "render/renderer/RendererFactory.h"

#include "render/renderer/benchmark/RendererForward.h"
#include "render/renderer/benchmark/RendererDeferred.h"
#include "render/renderer/benchmark/RendererVisBuf.h"
#include "render/renderer/benchmark/RendererVisBufDebug.h"
#include "render/renderer/benchmark/RendererVisBufGBuffer.h"
#include "render/renderer/benchmark/RendererRasterStats.h"
#include "render/renderer/benchmark/RendererDebugView.h"
#include "render/renderer/donut/RendererDonutDeferred.h"
#include "render/renderer/donut/RendererDonutRasterDebug.h"
#include "render/renderer/donut/RendererDonutVisDebug.h"
#include "render/renderer/donut/RendererDonutVisCompactDebug.h"
#include "render/renderer/donut/RendererDonutVisGBuffer.h"

namespace rndr {

    std::unique_ptr<RendererBase> create_renderer(
        uint32_t renderer_variant,
        uint32_t visibility_debug_mode) {

        const auto debug_mode =
            static_cast<VisibilityDebugMode>(visibility_debug_mode);
        switch (renderer_variant) {
        case  1: return std::make_unique<RendererForward>(false);
        case  2: return std::make_unique<RendererForward>(true);
        case  3: return std::make_unique<RendererDeferred>(false);
        case  4: return std::make_unique<RendererVisBuf>();
        case  5: return std::make_unique<RendererDeferred>(true);
        case  6: return std::make_unique<RendererVisBufGBuffer>();
        case  7: return std::make_unique<RendererDonutDeferred>(false);
        case  8: return std::make_unique<RendererDonutDeferred>(true);
        case  9: return std::make_unique<RendererDonutVisGBuffer>();
        case 10: return std::make_unique<RendererRasterStats>();
        case 11: return std::make_unique<RendererDebugView>();
        case 12: return std::make_unique<RendererVisBufDebug>(debug_mode);
        case 13: return std::make_unique<RendererDonutVisDebug>(debug_mode);
        case 14: return std::make_unique<RendererDonutRasterDebug>(debug_mode);
        case 15:
            util::Logger::g_logger.assert_with_log(
                visibility_debug_mode <= static_cast<std::uint32_t>(
                    DonutCompactDebugMode::GroupThread),
                "Donut compact debug mode must be between 0 and 3");
            return std::make_unique<RendererDonutVisCompactDebug>(
                static_cast<DonutCompactDebugMode>(visibility_debug_mode));
        default:
            util::Logger::g_logger.assert_with_log(false, "invalid renderer variant");
            return nullptr;
        }
    }

}
