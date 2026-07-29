#pragma once

#include <cstdint>

#include <DirectXCollision.h>

#include "scene/data/cpu/JungleSceneCPUData.h"
#include "scene/data/cpu/JungleSceneCPUDrawStream.h"

namespace scene {

    class JungleSceneCPUDrawStreamBuilder {
    public:
        static void build_all(
            const JungleSceneCPUData& scene,
            JungleSceneCPUDrawStream& stream);

        static void build_visible(
            const JungleSceneCPUData& scene,
            JungleSceneCPUDrawStream& stream,
            const DirectX::BoundingFrustum& frustum);

        static uint64_t count_indices(
            const JungleSceneCPUDrawStream& stream);
    };

} // namespace scene
