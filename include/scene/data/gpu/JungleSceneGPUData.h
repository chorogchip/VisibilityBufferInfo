#pragma once

#include <cstdint>

#include <DirectXMath.h>

#include "engine/GPUResource.h"
#include "scene/data/gpu/DonutSceneGPUData.h"

namespace scene {

    struct JungleSceneGPUData {
        struct PointPrototypeData {
            DirectX::XMFLOAT4X4 prototype_local_transform{};
            DirectX::XMFLOAT4X4 instancer_world_transform{};
        };

        eng::GPUResource point_instance_buffer;
        eng::GPUResource point_prototype_buffer;
        eng::GPUResource point_instance_id_buffer;
        uint32_t point_instance_count = 0;
        uint32_t point_prototype_count = 0;
        uint32_t point_instance_id_capacity = 0;
    };

    struct JungleDonutSceneGPUData {
        DonutSceneGPUData scene;
        JungleSceneGPUData point_scene;
    };

    static_assert(
        sizeof(JungleSceneGPUData::PointPrototypeData) == 128);

} // namespace scene
