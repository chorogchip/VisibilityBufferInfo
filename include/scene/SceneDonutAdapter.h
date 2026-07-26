#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "scene/SceneDataCPU.h"
#include "scene/donut/DonutSceneDataCPU.h"

namespace scene {

    std::unique_ptr<SceneDataCPU> convert_donut_scene_to_benchmark(
        std::unique_ptr<DonutSceneDataCPU> source,
        const std::filesystem::path& fallback_path,
        const std::string& importer_name);
}
