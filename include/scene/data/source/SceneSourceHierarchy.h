#pragma once

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "scene/data/source/SceneConstants.h"

namespace scene::source {

    // Matrices use DirectX row-vector order and are not transposed for shaders.
    // Empty instance_transforms means the mesh is placed once by
    // local_transform. Otherwise the mesh is the shared prototype for those
    // node-local instance transforms.
    struct Node {
        std::vector<uint32_t> children;
        DirectX::XMFLOAT4X4 local_transform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        uint32_t mesh_id = SceneConstants::INVALID_INDEX;
        std::vector<DirectX::XMFLOAT4X4> instance_transforms;

        void validate() const;
    };
}
