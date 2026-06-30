#include "SceneSynthSphere.h"

#include <random>
#include <assert.h>

SceneSynthSphere SceneSynthSphere::generate(const SceneInfo& info) {
	SceneSynthSphere ret{};
	
	std::random_device rd;
	std::mt19937 gen(rd());
	gen.seed(info.seed);

	std::uniform_real_distribution<float> dist_xy{ info.xy_min, info.xy_max };
	std::uniform_real_distribution<float> dist_z{ info.z_min, info.z_max };
	std::uniform_real_distribution<float> dist_radius{ info.radius_min, info.radius_max };
	
	assert(info.material_count > 0);
	ret.materials.init(info.material_count, info.material_float4_count);
	std::uniform_int_distribution<uint32_t> dist_material{ 0, ret.materials.get_material_count() - 1 };

	assert(info.geometry_count > 0);
	for (uint32_t i = 0; i < info.geometry_count; ++i) {
		ret.geometries.push_back(MeshGeometry::generate_sphere(info.geometry_division_min, info.geometry_division_max));
	}
	std::uniform_int_distribution<uint32_t> dist_geometry{ 0, ret.geometries.size() - 1 };

	assert(info.sphere_count > 0);
	for (uint32_t i = 0; i < info.sphere_count; ++i) {
		ret.instances.push_back(SphereInstance{
			DirectX::XMFLOAT3{ dist_xy(gen), dist_xy(gen), dist_z(gen) },
			dist_radius(gen),
			i,
			dist_material(gen),
			dist_geometry(gen),
			0
		});
	}

	return ret;
}
