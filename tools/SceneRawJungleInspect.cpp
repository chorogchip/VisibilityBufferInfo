#include "scene/raw/SceneRawJungle.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: SceneRawJungleInspect <JungleRuins_Karma.usda> [manifest.json]\n";
        return 2;
    }

    try {
        const std::filesystem::path root_path = argv[1];
        const std::filesystem::path manifest_path = argc == 3
            ? std::filesystem::path(argv[2])
            : root_path.parent_path() / "SceneRawJungleManifest.json";

        const auto scene = scene::raw::SceneRawJungle::open(root_path);
        scene->write_manifest(manifest_path);

        const auto& stats = scene->statistics();
        std::cout << "Manifest: " << manifest_path.string() << '\n';
        std::cout << "Prims: " << stats.prim_count
                  << ", properties: " << stats.property_count
                  << ", point instancers: " << stats.point_instancer_count
                  << ", native prototypes: " << stats.prototype_count
                  << '\n';
        std::cout << "Diagnostics: " << scene->diagnostics().size() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SceneRawJungle inspection failed: " << error.what() << '\n';
        return 1;
    }
}
