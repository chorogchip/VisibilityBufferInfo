#pragma once

#include <cstdint>

#include <DirectXCollision.h>

#include "scene/data/cpu/SceneCPUData.h"
#include "scene/data/cpu/SceneCPUDrawStream.h"

namespace scene {

    class SceneCPUDrawStreamBuilder {
    public:
        static void build_all(
            const SceneCPUData& scene,
            SceneCPUDrawStream& stream);
        static void build_visible(
            const SceneCPUData& scene,
            SceneCPUDrawStream& stream,
            const DirectX::BoundingFrustum& frustum);
        static uint64_t count_indices(const SceneCPUDrawStream& stream);
    };
}
