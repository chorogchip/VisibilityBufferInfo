#pragma once

namespace scene {

	enum class EnumSortType : uint32_t {
		RANDOM = 0,
		FROM_FRONT,
		FROM_BACK,
	};

	struct SceneInfoSphere {
		uint32_t seed;
		uint32_t object_count;
		uint32_t material_count;
		uint32_t mesh_count;
		EnumSortType sort_type;
		float z_min;
		float z_max;
		float xy_minmax;
		float radius;
		uint32_t mesh_division;
		uint32_t gbuffer_resource_count;  // g버퍼 장수	
	};
}