#pragma once

#include <d3d12.h>
#include <wrl.h>

#include "engine/GraphicsQueue.h"
#include "scene/SceneDataCPU.h"

namespace rndr {
    class VisBufResourceBuilder {
    public:
        static Microsoft::WRL::ComPtr<ID3D12Resource> build_mesh_buffer(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            ID3D12CommandAllocator* command_allocator,
            eng::GraphicsQueue& graphics_queue,
            ID3D12Resource* vertex_buffer,
            ID3D12Resource* index_buffer,
            ID3D12Resource* instance_buffer,
            const scene::SceneDataCPU* scene);
    };
}
