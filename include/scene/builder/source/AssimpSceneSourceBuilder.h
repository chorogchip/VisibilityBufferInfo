#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "scene/data/source/SceneSourceData.h"

namespace scene {

    struct AssimpSceneSourceBuildResult {
        std::unique_ptr<SceneSourceData> scene;
        std::string error_message;

        explicit operator bool() const noexcept {
            return scene != nullptr;
        }
    };

    // Builds renderer-independent source data from Sponza or Bistro assets.
    class AssimpSceneSourceBuilder {
    public:
        static AssimpSceneSourceBuildResult build(
            const std::filesystem::path& path);
    };
}
