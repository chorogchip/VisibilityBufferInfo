#pragma once

#include <limits>
#include <memory>

#include "scene/data/source/SceneSourceData.h"
#include "scene/raw/SceneRawJungle.h"

namespace scene {

    struct SceneRawJungleToSourceOptions {
        double time_code = std::numeric_limits<double>::quiet_NaN();
        bool include_inactive_prims = false;
        bool include_invisible_prims = true;
        bool include_proxy_purpose = false;
        bool include_guide_purpose = false;
        bool preserve_native_instances = true;
        bool preserve_point_instancers = true;
    };

    class SceneRawJungleToSource {
    public:
        static std::unique_ptr<SceneSourceData> build(
            const raw::SceneRawJungle& raw_scene,
            const SceneRawJungleToSourceOptions& options = {});
    };

} // namespace scene
