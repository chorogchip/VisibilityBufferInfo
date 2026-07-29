#include "scene/builder/source/SceneSourceFactory.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <string>

#include "ProgramArgument.h"
#include "scene/builder/source/BuilderAssimp.h"
#include "scene/builder/source/BuilderQuads.h"
#include "scene/builder/source/JungleSceneSourceBuilder.h"
#include "util/Logger.h"
#include "util/MyPath.h"

namespace scene {

    bool SceneSourceFactory::uses_jungle_builder(
        const util::ProgramArgument& argument) {

        if (!argument.to_use_scene) {
            return false;
        }
        if (argument.scene_importer == "jungle") {
            return true;
        }
        if (argument.scene_importer != "auto") {
            return false;
        }

        std::string extension =
            std::filesystem::path(
                argument.scene_path).extension().string();
        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        return extension == ".usd" || extension == ".usda";
    }

    std::unique_ptr<SceneSourceData> SceneSourceFactory::create_scene(const util::ProgramArgument& argument) {

        if (!argument.to_use_scene) {
            BuilderQuads::BuilderQuadsConfig config{};
            config.object_count = argument.object_count;
            config.overdraw_count = argument.overdraw_count;
            config.division = argument.geometry_div;
            return BuilderQuads::build(config);
        }

        const auto path = util::MyPath(argument.scene_path).get_absolute();
        util::Logger::g_logger.assert_with_log(path.is_regular(), "scene source not exist");

        if (uses_jungle_builder(argument)) {
            return JungleSceneSourceBuilder::build(path.get());
        }

        return BuilderAssimp::build(path.get());
    }
}
