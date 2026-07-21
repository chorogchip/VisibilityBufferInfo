#pragma once

#include <filesystem>

namespace scene {

    class DonutScenImporter {
    public:
        static void load(const std::filesystem::path& path);
    };
}
