#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "JungleSceneSourceValidation.h"

namespace {

    constexpr std::array<const char*, 4> PACKAGE_NAMES = {
        "jungle_global.glb",
        "jungle_cinematic.glb",
        "jungle_extended.glb",
        "jungle_pyramid.glb"
    };
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr
            << "Usage: JungleSceneSourceValidate <package-directory>\n";
        return 2;
    }

    const std::filesystem::path package_directory =
        std::filesystem::absolute(argv[1]).lexically_normal();
    std::vector<std::filesystem::path> package_paths;
    package_paths.reserve(PACKAGE_NAMES.size());
    for (const char* package_name : PACKAGE_NAMES) {
        const std::filesystem::path path =
            package_directory / package_name;
        if (!std::filesystem::is_regular_file(path)) {
            std::cerr << "Missing Jungle package: " << path << '\n';
            return 2;
        }
        package_paths.push_back(path);
    }

    std::string error_message;
    if (!jungle::validation::validate_package_set(
            package_paths,
            std::cout,
            error_message)) {
        std::cerr << "Jungle source validation failed: "
                  << error_message << '\n';
        return 1;
    }
    return 0;
}
