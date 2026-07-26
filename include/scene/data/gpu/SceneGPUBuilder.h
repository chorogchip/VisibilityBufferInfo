#pragma once

#include <vector>

#include <d3d12.h>
#include <wrl.h>

#include "scene/data/cpu/SceneCPUData.h"
#include "scene/data/gpu/SceneGPUData.h"

namespace scene {

    class SceneGPUBuilder {
    public:
        static SceneGPUData build(
            const SceneCPUData& source,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
                used_upload_heaps,
            bool load_textures = true);
    };
}
