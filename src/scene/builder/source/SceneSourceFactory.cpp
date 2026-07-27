#include "scene/builder/source/SceneSourceFactory.h"

#include <memory>

#include "ProgramArgument.h"
#include "util/Logger.h"
#include "util/MyPath.h"
#include "scene/builder/source/synth/SyntheticQuads.h"
#include "scene/builder/source/AssimpSceneSourceBuilder.h"
#include "scene/builder/source/JungleSceneSourceBuilder.h"

namespace scene {

    std::unique_ptr<SceneSourceData> SceneSourceFactory::create_scene(const util::ProgramArgument& argument) {

        if (!argument.to_use_scene) {
            SyntheticQuads::SyntheticQuadsConfig config{};
            config.object_count = argument.object_count;
            config.overdraw_count = argument.overdraw_count;
            config.division = argument.geometry_div;
            return SyntheticQuads::build(config);
        }

        const auto path = util::MyPath(argument.scene_path).get_absolute();
        util::Logger::g_logger.assert_with_log(path.is_regular(), "scene source not exist");

        if (argument.scene_importer == "jungle" ||
            (argument.scene_importer == "auto" && path.is_lower_contain("jungle"))) {
            return JungleSceneSourceBuilder::build(path.get());
        }

        return AssimpSceneSourceBuilder::build(path.get());
    }
}