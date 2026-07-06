#include "SceneSynthSphere.h"

#include <random>
#include <assert.h>

std::unique_ptr<SceneSynthSphere> SceneSynthSphere::generate(const scene::SceneInfoSphere& info) {
	std::unique_ptr<SceneSynthSphere> ret{ new SceneSynthSphere() };
	
	std::mt19937 gen(info.seed);

	std::uniform_real_distribution<float> dist_xy{ -info.xy_minmax, info.xy_minmax };
	std::uniform_real_distribution<float> dist_z{ info.z_min, info.z_max };
	
	assert(info.material_count > 0);
	ret->materials.init(info.material_count, info.gbuffer_resource_count);
	std::uniform_int_distribution<uint32_t> dist_material{ 0, ret->materials.get_material_count() - 1 };

	assert(info.mesh_count > 0);
	for (uint32_t i = 0; i < info.mesh_count; ++i) {
		ret->geometries.push_back(MeshGeometry::generate_sphere(info.mesh_division));
	}
	std::uniform_int_distribution<uint32_t> dist_geometry{ 0, static_cast<uint32_t>(ret->geometries.size() - 1) };

	assert(info.sphere_count > 0);
	for (uint32_t i = 0; i < info.sphere_count; ++i) {
		ret->instances.push_back(SphereInstance{
			DirectX::XMFLOAT3{ dist_xy(gen), dist_xy(gen), dist_z(gen) },
			info.radius,
			i,
			dist_material(gen),
			dist_geometry(gen),
			0
		});
	}

	return ret;
}

