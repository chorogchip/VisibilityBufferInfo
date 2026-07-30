#pragma once

#include "render/renderer/donut/DonutCompactDebugMode.h"
#include "render/renderer/donut/RendererDonutVisGBuffer.h"

namespace rndr {

    class RendererDonutVisCompactDebug final :
        public RendererDonutVisGBuffer {
    public:
        explicit RendererDonutVisCompactDebug(
            DonutCompactDebugMode compact_debug_mode)
            : RendererDonutVisGBuffer(compact_debug_mode) {}
        ~RendererDonutVisCompactDebug() override = default;
    };

}
