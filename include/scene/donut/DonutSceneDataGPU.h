#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <d3d12.h>
#include <wrl.h>

#include "scene/donut/DonutSceneDataCPU.h"

namespace scene {

    struct DonutSceneDataGPU {
        struct VertexLayout {
            uint32_t position_offset = 0;
            uint32_t prev_position_offset = 0;
            uint32_t texcoord_offset = 0;
            uint32_t normal_offset = 0;
            uint32_t tangent_offset = 0;
            uint32_t byte_size = 0;
        };

        struct Draw {
            uint32_t index_count = 0;
            uint32_t start_index_location = 0;
            int32_t base_vertex_location = 0;
            uint32_t start_instance_location = 0;
            uint32_t start_vertex_location = 0;
            uint32_t material_index = 0;
        };

        Microsoft::WRL::ComPtr<ID3D12Resource> vertex_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> index_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> geometry_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> instance_buffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> material_buffer;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> material_constant_buffers;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> textures;

        D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
        VertexLayout vertex_layout{};
        std::vector<dnt::GeometryData> geometry_data;
        std::vector<dnt::MaterialConstants> material_data;
        std::vector<std::array<uint32_t, material_texture_slot_count>> material_texture_resources;
        std::vector<Draw> draws;
    };
}
