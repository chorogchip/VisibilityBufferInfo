#include "render/RendererFactory.h"

#include "render/RendererDeferred.h"
#include "render/RendererDeferredPrepass.h"
#include "render/RendererForward.h"
#include "render/RendererForwardPrepass.h"
#include "render/RendererTVB.h"
#include "render/RendererTVBGBuffer.h"

namespace rndr {

    std::unique_ptr<RendererBase> create_renderer(uint32_t renderer_variant) {
        switch (renderer_variant) {
        case 1:
        default:
            return std::make_unique<RendererForward>();
        case 2:
            return std::make_unique<RendererForwardPrePass>();
        case 3:
            return std::make_unique<RendererDeferred>();
        case 4:
            return std::make_unique<RendererTVB>();
        case 5:
            return std::make_unique<RendererDeferredPrepass>();
        case 6:
            return std::make_unique<RendererTVBGBuffer>();
        }
    }

}
