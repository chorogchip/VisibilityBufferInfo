#pragma once

#include <filesystem>
#include <memory>

#include "scene/data/source/SceneSourceData.h"

namespace scene {

    class BuilderAssimp {

    public:
        static std::unique_ptr<SceneSourceData> build(const std::filesystem::path& path);
    };
}
