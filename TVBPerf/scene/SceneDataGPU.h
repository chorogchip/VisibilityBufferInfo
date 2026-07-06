#pragma once

#include <d3d12.h>
#include <wrl.h>

#include "scene/SceneDataCPU.h"

namespace scene {
	class SceneDataGPU {
	public:
		Microsoft::WRL::ComPtr<ID3D12Resource> vertex_buffer;
		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};

		Microsoft::WRL::ComPtr<ID3D12Resource> index_buffer;
		D3D12_INDEX_BUFFER_VIEW index_buffer_view{};

		Microsoft::WRL::ComPtr<ID3D12Resource> object_buffer;
	};
}