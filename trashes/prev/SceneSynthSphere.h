#pragma once

#include <cstdint>
#include <memory>
#include <DirectXMath.h>

#include "scene/Materials.h"
#include "scene/Meshgeometry.h"
#include "scene/SceneInfoSphere.h"

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

	std::vector<SphereInstance> instances;
	Materials materials{};
	std::vector<MeshGeometry> geometries;

	static std::unique_ptr<SceneSynthSphere> generate(const scene::SceneInfoSphere& info);

};

static_assert(sizeof(SceneSynthSphere::SphereInstance) == 32, "size error");
