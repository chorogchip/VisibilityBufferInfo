#pragma once

#include <vector>

#include <d3d12.h>
#include <wrl.h>

#include "scene/data/cpu/SceneCPUData.h"
#include "scene/data/gpu/BenchmarkSceneGPUData.h"

namespace scene {

    // Allocates and uploads the draw-ready CPU scene to D3D12 resources.
    class BenchmarkSceneGPUBuilder {
    public:
        static BenchmarkSceneGPUData build(
            const SceneCPUData& source,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
                used_upload_heaps,
            bool load_textures = true);

    };
}
