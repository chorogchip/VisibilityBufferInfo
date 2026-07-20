#pragma once

#include <filesystem>
#include <memory>

#include "scene/donut/DonutSceneDataCPU.h"

namespace scene::donut {

    class DonutSceneAssimpImporter {
    public:
        static std::unique_ptr<DonutSceneDataCPU> load(const std::filesystem::path& path);
    };
}
