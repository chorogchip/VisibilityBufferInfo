#include "render/renderer/RendererFactory.h"

#include "render/renderer/benchmark/RendererForward.h"
#include "render/renderer/benchmark/RendererDeferred.h"
#include "render/renderer/benchmark/RendererVisBuf.h"
#include "render/renderer/benchmark/RendererVisBufGBuffer.h"
#include "render/renderer/donut/RendererDonutDeferred.h"

namespace rndr {

    std::unique_ptr<RendererBase> create_renderer(uint32_t renderer_variant) {
        switch (renderer_variant) {
        case 1:
        default:
            return std::make_unique<RendererForward>(false);
        case 2:
            return std::make_unique<RendererForward>(true);
        case 3:
            return std::make_unique<RendererDeferred>(false);
        case 4:
            return std::make_unique<RendererVisBuf>();
        case 5:
            return std::make_unique<RendererDeferred>(true);
        case 6:
            return std::make_unique<RendererVisBufGBuffer>();
        case 7:
            return std::make_unique<RendererDonutDeferred>(false);
        case 8:
            return std::make_unique<RendererDonutDeferred>(true);
        }
    }

}
