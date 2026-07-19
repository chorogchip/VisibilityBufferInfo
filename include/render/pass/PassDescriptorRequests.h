#pragma once

#include "engine/ResourceManagerShader.h"
#include "render/VisBufResources.h"

namespace rndr {
    void request_material_textures(
        eng::ResourceManagerShader& manager,
        const scene::SceneResources& resources);

    void request_visbuf_scene(
        eng::ResourceManagerShader& manager,
        const VisBufResources& resources);
}
