#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "scene/data/source/SceneSourceData.h"

namespace scene {

    struct SceneSourceLoadResult {
        std::unique_ptr<SceneSourceData> scene;
        std::string error_message;

        explicit operator bool() const noexcept {
            return scene != nullptr;
        }
    };

    // Decodes a glTF/GLB into the renderer-independent source contract.
    // Jungle EXT_mesh_gpu_instancing streams and extras.jr hierarchy metadata
    // are retained instead of being expanded into render objects.
    class SceneSourceGltfLoader {
    public:
        static SceneSourceLoadResult load(
            const std::filesystem::path& path);
    };
}
