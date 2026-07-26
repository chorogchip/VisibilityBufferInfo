#pragma once

#include <cstdint>

#include <DirectXCollision.h>

#include "scene/data/cpu/SceneCPUData.h"

namespace scene {

    class SceneCPUDrawBuilder {
    public:
        static void reset(SceneCPUData& scene);
        static void build_visible(
            SceneCPUData& scene,
            const DirectX::BoundingFrustum& frustum);
        static uint64_t count_indices(const SceneCPUData& scene);
    };
}
