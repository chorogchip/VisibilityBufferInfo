#include "scene/builder/source/JungleSceneSourceBuilder.h"

#include "scene/builder/source/SceneRawJungleToSource.h"
#include "scene/raw/SceneRawJungle.h"

namespace scene {

    std::unique_ptr<SceneSourceData> JungleSceneSourceBuilder::build(
        const std::filesystem::path& path) {

        const auto raw_scene = raw::SceneRawJungle::open(path);
        return SceneRawJungleToSource::build(*raw_scene);
    }

} // namespace scene
