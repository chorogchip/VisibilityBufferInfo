#pragma once

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "scene/data/cpu/SceneCPUData.h"

namespace scene {

    struct JungleSceneCPUData {
        // One compact authored PointInstancer entry. Prototype and instancer
        // affine transforms stay in PointPrototype so this remains 48 bytes.
        struct PointInstance {
            DirectX::XMFLOAT3 translation{};
            uint32_t source_index = 0;
            DirectX::XMFLOAT4 rotation =
                { 0.0f, 0.0f, 0.0f, 1.0f };
            DirectX::XMFLOAT3 scale =
                { 1.0f, 1.0f, 1.0f };
            uint32_t pad0 = 0;
        };

        // IDs in [first_instance_id, first_instance_id + instance_count)
        // index point_instances. Geometry is shared through mesh_id.
        struct PointPrototype {
            uint32_t mesh_id = source::SceneConstants::INVALID_INDEX;
            uint32_t first_instance_id = 0;
            uint32_t instance_count = 0;
            uint32_t point_instancer_id =
                source::SceneConstants::INVALID_INDEX;
            DirectX::XMFLOAT4X4 prototype_local_transform = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };
            DirectX::XMFLOAT4X4 instancer_world_transform = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };
        };

        // Ordinary/native instances and all shared triangle geometry.
        // PointInstancer logical instances are deliberately absent.
        SceneCPUData scene;
        std::vector<PointInstance> point_instances;
        std::vector<uint32_t> point_instance_ids_by_prototype;
        std::vector<PointPrototype> point_prototypes;
        uint64_t logical_point_instance_count = 0;
    };

    static_assert(sizeof(JungleSceneCPUData::PointInstance) == 48);
    static_assert(sizeof(JungleSceneCPUData::PointPrototype) == 144);

} // namespace scene
