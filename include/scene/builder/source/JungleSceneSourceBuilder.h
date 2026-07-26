#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "scene/data/source/SceneSourceData.h"

namespace scene {

    struct JungleSceneSourceBuildResult {
        std::unique_ptr<SceneSourceData> scene;
        std::string error_message;

        explicit operator bool() const noexcept {
            return scene != nullptr;
        }
    };

    // Builds renderer-independent source data from a Jungle region GLB.
    // EXT_mesh_gpu_instancing and extras.jr are required Jungle inputs.
    class JungleSceneSourceBuilder {
    public:
        static JungleSceneSourceBuildResult build(
            const std::filesystem::path& path);
    };
}
