#pragma once

#include <memory>

#include "scene/SceneInfo.h"
#include "scene/SceneDataCPU.h"

namespace scene {
	class SceneBuilder {
	public:
		static std::unique_ptr<SceneDataCPU> build(const SceneInfoSphere& info);
	};
}