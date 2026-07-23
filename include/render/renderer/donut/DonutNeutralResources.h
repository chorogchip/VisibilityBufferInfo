#pragma once

#include <vector>

#include <d3d12.h>
#include <wrl.h>

#include "engine/GPUResource.h"

namespace rndr {

    struct DonutNeutralResources {
        eng::GPUResource black_texture;
        eng::GPUResource white_texture;
        eng::GPUResource black_cube_map_array;
        eng::GPUResource black_depth_stencil_texture_2d_array;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> depth_clear_dsv_heap;

        void init(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps);
    };
}
