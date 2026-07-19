#pragma once

#include <d3d12.h>

#include "scene/SceneResources.h"

namespace rndr {
    struct VisBufResources {
        ID3D12Resource* visibility = nullptr;
        ID3D12Resource* mesh_buffer = nullptr;
        scene::SceneResources scene{};
    };
}
