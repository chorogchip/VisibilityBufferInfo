#pragma once

#include <filesystem>
#include <memory>

#include "scene/data/source/SceneSourceData.h"

namespace scene {

    // Builds renderer-independent source data from a Jungle region GLB.
    // EXT_mesh_gpu_instancing and extras.jr are required Jungle inputs.
    // Invalid canonical input terminates through the project Logger.
    class JungleSceneSourceBuilder {
    public:
        static std::unique_ptr<SceneSourceData> build(
            const std::filesystem::path& path);
    };
}
