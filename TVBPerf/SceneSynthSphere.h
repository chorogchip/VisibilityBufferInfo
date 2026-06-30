#pragma once

#include <cstdint>
#include <cstddef>
#include <DirectXMath.h>

#include "Materials.h"
#include "MeshGeometry.h"

class SceneSynthSphere
{
private:
	SceneSynthSphere() = default;
public:

	struct SphereInstance {
		DirectX::XMFLOAT3 center;
		float radius;
		uint32_t object_id;
		uint32_t material_id;
		// TVB 에 지나치게 유리한 상황을 막기 위해 어떤 인스턴스들은 내용은 같지만 서로 다른 버퍼 사용
		uint32_t geometry_id;
		uint32_t flags;
	};

	struct SceneInfo {
		uint32_t seed;
		uint32_t sphere_count;
		uint32_t material_count;
		uint32_t geometry_count;
		float z_min;
		float z_max;
		float xy_min;
		float xy_max;
		float radius_min;
		float radius_max;
		uint32_t geometry_division_min;
		uint32_t geometry_division_max;
		uint32_t material_float4_count;
	};

	std::vector<SphereInstance> instances;
	Materials materials;
	std::vector<MeshGeometry> geometries;

	static SceneSynthSphere generate(const SceneInfo& info);
};

static_assert(sizeof(SceneSynthSphere::SphereInstance) == 32, "size error");
