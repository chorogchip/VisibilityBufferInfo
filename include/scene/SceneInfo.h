#pragma once

namespace scene {

	struct SceneInfoSphere {
		uint32_t seed;
		uint32_t object_count;
		uint32_t material_count;
		uint32_t mesh_count;
		uint32_t overdraw_count;
		float z_min;
		float z_max;
		float xy_minmax;
		float radius;
		uint32_t mesh_division;
		uint32_t gbuffer_resource_count;  // g버퍼 장수	
	};
}