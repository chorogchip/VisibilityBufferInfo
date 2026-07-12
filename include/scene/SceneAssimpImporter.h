#pragma once

#include <filesystem>
#include <memory>

#include "scene/SceneDataCPU.h"

namespace scene {
	class SceneAssimpImporter {
	public:
		static std::unique_ptr<SceneDataCPU> load(const std::filesystem::path& path);
	};
}