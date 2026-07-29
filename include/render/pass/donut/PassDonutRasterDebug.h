#pragma once

#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include "render/renderer/VisibilityDebugMode.h"
#include "scene/data/cpu/SceneCPUDrawStream.h"
#include "scene/data/gpu/DonutSceneGPUData.h"
#include "util/Constants.h"

namespace eng {
    class GPUResource;
    class ResourceManagerFrame;
    class ResourceManagerSampler;
    class ResourceManagerShader;
}

namespace rndr {

    struct PassDonutRasterDebugResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerSampler* sampler_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* back_buffers[util::Constants::FRAME_COUNT]{};
        eng::GPUResource* depth = nullptr;
        eng::GPUResource* constant_buffers[util::Constants::FRAME_COUNT]{};
        const scene::DonutSceneGPUData* scene = nullptr;
        const scene::SceneCPUDrawStream* draw_stream = nullptr;
    };

    class PassDonutRasterDebug {
    public:
        static constexpr UINT MATERIAL_TEXTURE_DESCRIPTOR_COUNT =
            scene::DonutSceneGPUData::MATERIAL_TEXTURE_DESCRIPTOR_COUNT;

        void init(
            ID3D12Device* device,
            const util::ProgramArgument& arguments,
            VisibilityDebugMode mode,
            const PassDonutRasterDebugResources& resources);

        void render(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            const D3D12_VIEWPORT& viewport,
            const D3D12_RECT& scissor_rect);

    private:
        PassDonutRasterDebugResources resources_{};
        eng::GraphicsPipeline pso_;
    };

}
