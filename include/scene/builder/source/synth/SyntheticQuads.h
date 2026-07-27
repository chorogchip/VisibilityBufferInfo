#pragma once

#include <cstdint>
#include <memory>

#include "scene/data/source/SceneSourceData.h"

namespace scene {

	class SyntheticQuads {

	public:
		struct SyntheticQuadsConfig {
			uint32_t object_count;
			uint32_t overdraw_count;
			uint32_t division;
		};

		static std::unique_ptr<SceneSourceData> build(const SyntheticQuadsConfig& config);
	};
}