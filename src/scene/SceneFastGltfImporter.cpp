#include "scene/SceneFastGltfImporter.h"

#include "scene/SceneDonutAdapter.h"
#include "scene/donut/DonutSceneFastGltfImporter.h"

namespace scene {

    std::unique_ptr<SceneDataCPU> SceneFastGltfImporter::load(
        const std::filesystem::path& path) {
        return convert_donut_scene_to_benchmark(
            donut::DonutSceneFastGltfImporter::load(path),
            path,
            "fastgltf");
    }
}
