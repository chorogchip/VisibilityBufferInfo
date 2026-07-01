#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>

#include "SceneSynthSphere.h"

class SceneSynthSphereRuntime
{
private:
	SceneSynthSphereRuntime() = default;

public:

	struct InstanceData {
		DirectX::XMFLOAT4X4 mat_world;
	};
	std::vector<InstanceData> instances_datas;

	struct MeshBufferHandle {
		uint32_t index_count;
		uint32_t start_index_offset;
		uint32_t base_vertex_offset;
	};
	std::vector<MeshBufferHandle> geometries_handles;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertex_buffer;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};

	Microsoft::WRL::ComPtr<ID3D12Resource> index_buffer;
	D3D12_INDEX_BUFFER_VIEW index_buffer_view{};

	static std::unique_ptr<SceneSynthSphereRuntime> generate(const SceneSynthSphere& scene, ID3D12Device* p_device);
};

