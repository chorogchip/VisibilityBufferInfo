#pragma once

#include <filesystem>
#include <memory>

#include "scene/data/source/SceneSourceData.h"

namespace scene {

    // Opens a composed Jungle USD stage and preserves its USD-specific
    // semantics in SceneSourceData. Triangle/render materialization is a
    // separate JungleSceneCPUBuilder step.
    class JungleSceneSourceBuilder {
    public:
        static std::unique_ptr<SceneSourceData> build(
            const std::filesystem::path& path);
    };

} // namespace scene
