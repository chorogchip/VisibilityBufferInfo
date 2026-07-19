#pragma once

#include "Constants.h"
#include "ProgramArgument.h"
#include "engine/GraphicsPipeline.h"
#include <vector>
#include "scene/SceneDataCPU.h"

namespace eng { class GPUResource; class ResourceManagerFrame; class ResourceManagerSampler; class ResourceManagerShader; }

namespace rndr {
    struct PassVisBufResolveResources {
        eng::ResourceManagerFrame* frame_manager = nullptr;
        eng::ResourceManagerShader* shader_manager = nullptr;
        eng::GPUResource* back_buffers[util::FRAME_COUNT]{};
        eng::GPUResource* visibility = nullptr;
        ID3D12Resource* vertex_buffer = nullptr;
        ID3D12Resource* index_buffer = nullptr;
        ID3D12Resource* mesh_buffer = nullptr;
        ID3D12Resource* instance_buffer = nullptr;
        ID3D12Resource* material_buffer = nullptr;
        std::vector<ID3D12Resource*> material_textures;
        const scene::SceneDataCPU* scene = nullptr;
        ID3D12Resource* constant_buffers[util::FRAME_COUNT]{};
        eng::ResourceManagerSampler* sampler_manager = nullptr;
    };

    class PassVisBufResolve {
    public:
        void init(ID3D12Device*, const util::ProgramArgument&, const PassVisBufResolveResources&);
        void render(ID3D12GraphicsCommandList*, UINT, const D3D12_VIEWPORT&, const D3D12_RECT&);
    private:
        PassVisBufResolveResources resources_{};
        eng::GraphicsPipeline pso_;
    };
}
