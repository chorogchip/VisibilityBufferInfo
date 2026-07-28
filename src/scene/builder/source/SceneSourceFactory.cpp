#include "scene/builder/source/SceneSourceFactory.h"

#include <memory>

#include "ProgramArgument.h"
#include "util/Logger.h"
#include "util/MyPath.h"
#include "scene/builder/source/BuilderQuads.h"
#include "scene/builder/source/BuilderAssimp.h"

namespace scene {

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

        util::Logger::g_logger.assert_with_log(
            argument.scene_importer != "jungle",
            "Jungle USD currently stops at SceneSource; SceneDataCPU conversion is not implemented.");

        return BuilderAssimp::build(path.get());
    }
}
