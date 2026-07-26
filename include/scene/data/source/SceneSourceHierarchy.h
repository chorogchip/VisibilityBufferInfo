#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "math/AABB.h"
#include "scene/data/source/SceneConstants.h"

namespace scene::source {

    enum class NodeKind : uint8_t {
        Generic,
        SceneRoot,
        Region,
        Cell,
        System,
        InstanceSet,
        StaticObject
    };

    enum class Region : uint8_t {
        None,
        Global,
        Cinematic,
        Extended,
        Pyramid
    };

    // Compact glTF TRS plus the reversible source-array index used by the
    // Jungle USD point-instancer pipeline.
    struct InstanceTransform {
        DirectX::XMFLOAT3 translation = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
        uint32_t source_index = SceneConstants::INVALID_INDEX;
    };

    static_assert(sizeof(InstanceTransform) == 44);

    // Matrices use DirectX row-vector order and are not transposed for shaders.
    // An instance range belongs to this node and uses node-local transforms.
    struct Node {
        std::vector<uint32_t> children;
        DirectX::XMFLOAT4X4 local_transform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        uint32_t mesh_id = SceneConstants::INVALID_INDEX;
        uint32_t camera_id = SceneConstants::INVALID_INDEX;
        uint32_t first_instance = 0;
        uint32_t instance_count = 0;

        NodeKind kind = NodeKind::Generic;
        Region region = Region::None;
        math::AABB world_bounds{};
        std::string stable_id;

        void validate() const;
    };
}
