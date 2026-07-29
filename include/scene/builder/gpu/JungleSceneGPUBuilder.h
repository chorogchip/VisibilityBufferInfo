#pragma once

#include <functional>
#include <vector>

#include <d3d12.h>
#include <wrl.h>

#include "scene/data/cpu/SceneCPUData.h"
#include "scene/data/gpu/BenchmarkSceneGPUData.h"
#include "scene/data/gpu/DonutSceneGPUData.h"

namespace scene {

    // Dedicated Jungle GPU entry point. The initial implementation reuses the
    // proven generic upload layouts while keeping a stable extension boundary
    // for GPU-native prototype/PointInstancer data later.
    class JungleSceneGPUBuilder {
    public:
        static BenchmarkSceneGPUData build_benchmark(
            const SceneCPUData& source,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
                used_upload_heaps,
            bool load_textures = true);

        static DonutSceneGPUData build_donut(
            const SceneCPUData& source,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
                used_upload_heaps,
            const std::function<void()>& flush_uploads = {},
            bool load_textures = true);
    };

} // namespace scene
