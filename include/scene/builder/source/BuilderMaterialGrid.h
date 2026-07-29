#pragma once

#include <cstdint>
#include <memory>

#include "scene/data/source/SceneSourceData.h"

namespace scene {

	class BuilderMaterialGrid {

	public:
		struct BuilderMaterialGridConfig {
			uint32_t seed;
			uint32_t triangle_division;
			uint32_t material_count;
			uint32_t material_class_count;
			float material_locality;
			float material_diversity;
		};

		static std::unique_ptr<SceneSourceData> build(const BuilderMaterialGridConfig& config);
	};
}
