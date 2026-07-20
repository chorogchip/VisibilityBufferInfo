#include "render/renderer/donut/RendererDonut.h"

namespace rndr {

    RendererDonut::~RendererDonut() = default;

    void RendererDonut::init_shader_resources_() {
        resource_manager_shader_.init(
            device_.Get(),
            static_cast<UINT>(eng::ResourceManagerShader::EnumDescPos::COUNT));
    }

}
