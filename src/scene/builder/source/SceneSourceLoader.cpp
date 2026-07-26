#include "scene/builder/source/SceneSourceLoader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <string>

#include "scene/builder/source/AssimpSceneSourceBuilder.h"
#include "scene/builder/source/JungleSceneSourceBuilder.h"
#include "scene/builder/source/SyntheticSceneSourceBuilder.h"
#include "util/Logger.h"

namespace scene {

    namespace {

        std::filesystem::path absolute_path(
            const std::filesystem::path& path) {
            return path.is_absolute()
                ? path.lexically_normal()
                : std::filesystem::absolute(path).lexically_normal();
        }

        bool is_jungle_path(const std::filesystem::path& path) {
            std::string value = path.generic_string();
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::tolower(character));
                });
            return value.find("jungle") != std::string::npos;
        }

        template <typename Result>
        SceneSourceData take_scene(Result&& result) {
            if constexpr (requires {
                result.scene;
                result.error_message;
            }) {
                util::Logger::g_logger.assert_with_log(
                    static_cast<bool>(result),
                    result.error_message.c_str());
                return std::move(*result.scene);
            } else {
                util::Logger::g_logger.assert_with_log(
                    result != nullptr,
                    "Scene source build failed.");
                return std::move(*result);
            }
        }

        SceneSourceData load_assimp(
            const std::filesystem::path& path) {
            auto result = AssimpSceneSourceBuilder::build(path);
            return take_scene(std::move(result));
        }

        SceneSourceData load_jungle(
            const std::filesystem::path& path) {
            auto result = JungleSceneSourceBuilder::build(path);
            return take_scene(std::move(result));
        }
    }

    SceneSourceData SceneSourceLoader::load(
        const util::ProgramArgument& argument) {
        if (!argument.to_use_scene) {
            SyntheticPlaneConfig config{};
            config.object_count = argument.object_count;
            config.overdraw_count = argument.overdraw_count;
            config.division = argument.geometry_div;
            config.to_remain_only_in_camera =
                argument.to_remain_only_in_camera;
            return SyntheticSceneSourceBuilder(config).build();
        }

        const std::filesystem::path path =
            absolute_path(argument.scene_path);
        util::Logger::g_logger.assert_with_log(
            std::filesystem::is_regular_file(path),
            "Scene source file does not exist.");

        if (argument.scene_importer == "jungle" ||
            (argument.scene_importer == "auto" &&
                is_jungle_path(path))) {
            return load_jungle(path);
        }
        return load_assimp(path);
    }
}
