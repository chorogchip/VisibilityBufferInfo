#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <vector>

#include "scene/SceneDataCPU.h"
#include "engine/MaterialGPU.h"

namespace scene {
	class SceneDataGPU {
	public:
		struct MeshMetadata {
			uint32_t vertex_start = 0;
			uint32_t index_start = 0;
		};
		static_assert(sizeof(MeshMetadata) == 8);

		Microsoft::WRL::ComPtr<ID3D12Resource> vertex_buffer;
		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};

		Microsoft::WRL::ComPtr<ID3D12Resource> index_buffer;
		D3D12_INDEX_BUFFER_VIEW index_buffer_view{};

		Microsoft::WRL::ComPtr<ID3D12Resource> object_buffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> material_buffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> mesh_buffer;
		std::vector<eng::MaterialGPU> materials;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> textures;
	};
}
