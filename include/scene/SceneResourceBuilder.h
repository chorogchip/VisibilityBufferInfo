#pragma once

#include <memory>
#include <vector>
#include <d3d12.h>

#include "scene/SceneDataCPU.h"
#include "scene/SceneDataGPU.h"

namespace scene {
	class SceneResourceBuilder {
	public:
		static std::unique_ptr<SceneDataGPU> build(
			const SceneDataCPU& src,
			ID3D12Device* p_device,
			ID3D12GraphicsCommandList* p_list,
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps,
			bool to_load_textures = false);
	};
}
