#pragma once

#include <memory>
#include <cstdint>

class RendererBase;

namespace rndr {

    std::unique_ptr<RendererBase> create_renderer(uint32_t renderer_variant);

}
