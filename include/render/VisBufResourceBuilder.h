#pragma once

#include <d3d12.h>
#include <wrl.h>

#include "dx_util/Fence.h"
#include "scene/SceneResources.h"

namespace rndr {
    class VisBufResourceBuilder {
    public:
        static Microsoft::WRL::ComPtr<ID3D12Resource> build_mesh_buffer(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            ID3D12CommandAllocator* command_allocator,
            ID3D12CommandQueue* command_queue,
            dxutl::Fence& fence,
            const scene::SceneResources& scene);
    };
}
