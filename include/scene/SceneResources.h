#pragma once

#include <d3d12.h>
#include <vector>

#include "scene/SceneDataCPU.h"

namespace scene {
    struct SceneResources {
        ID3D12Resource* vertex_buffer = nullptr;
        ID3D12Resource* index_buffer = nullptr;
        ID3D12Resource* instance_buffer = nullptr;
        ID3D12Resource* material_buffer = nullptr;
        std::vector<ID3D12Resource*> material_textures;
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
        D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
        const SceneDataCPU* cpu = nullptr;
    };
}
