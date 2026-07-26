#pragma once

#include <cstdint>

#include "scene/data/source/SceneSourceData.h"

namespace scene {

    struct SyntheticPlaneConfig {
        uint32_t object_count = 1;
        uint32_t overdraw_count = 0;
        uint32_t division = 1;
        bool to_remain_only_in_camera = false;
    };

    class SyntheticSceneSourceBuilder {
    public:
        explicit SyntheticSceneSourceBuilder(
            const SyntheticPlaneConfig& config);

        SceneSourceData build() const;

    private:
        source::Primitive build_primitive_() const;
        void build_instances_(
            SceneSourceData& scene,
            source::Node& node) const;

        SyntheticPlaneConfig config_;
    };
}
