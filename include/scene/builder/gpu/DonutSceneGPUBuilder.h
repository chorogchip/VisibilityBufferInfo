#pragma once

#include <functional>
#include <vector>

#include <d3d12.h>
#include <wrl.h>

#include "scene/data/cpu/SceneCPUData.h"
#include "scene/data/gpu/DonutSceneGPUData.h"

namespace scene {

    class DonutSceneGPUBuilder {
    public:
        static DonutSceneGPUData build(
            const SceneCPUData& source,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
                used_upload_heaps,
            const std::function<void()>& flush_uploads = {},
            bool load_textures = true);

    };
}
