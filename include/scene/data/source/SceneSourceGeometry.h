#pragma once

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

#include "scene/data/source/SceneConstants.h"

namespace scene::source {

    struct Primitive {
        std::vector<DirectX::XMFLOAT3> positions;
        std::vector<DirectX::XMFLOAT3> normals;
        std::vector<DirectX::XMFLOAT4> tangents;
        std::vector<DirectX::XMFLOAT2> uv0;
        std::vector<uint32_t> indices;

        uint32_t material_id = SceneConstants::INVALID_INDEX;

        void validate() const;
    };

    struct Mesh {
        std::vector<Primitive> primitives;

        void validate() const;
    };
}
