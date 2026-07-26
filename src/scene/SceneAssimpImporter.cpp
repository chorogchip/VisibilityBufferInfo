#include "scene/SceneAssimpImporter.h"

#include "scene/SceneDonutAdapter.h"
#include "scene/donut/DonutSceneAssimpImporter.h"

namespace scene {

    std::unique_ptr<SceneDataCPU> SceneAssimpImporter::load(
        const std::filesystem::path& path) {
        return convert_donut_scene_to_benchmark(
            donut::DonutSceneAssimpImporter::load(path),
            path,
            "Assimp");
    }
}
