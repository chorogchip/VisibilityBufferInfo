#pragma once

#include <memory>
#include <vector>

#include <d3d12.h>
#include <wrl.h>

#include "scene/donut/DonutSceneDataCPU.h"
#include "scene/donut/DonutSceneDataGPU.h"

namespace scene::donut {

    class DonutSceneResourceBuilder {
    public:
        static void rebuild_draw_stream(
            const DonutSceneDataCPU& source,
            const std::vector<DonutSceneDataCPU::VisibleDraw>& visible_draws,
            DonutSceneDataGPU& destination);

        static std::unique_ptr<DonutSceneDataGPU> build(
            const DonutSceneDataCPU& source,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps,
            bool load_textures = true);
    };
}
