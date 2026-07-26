#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "scene/data/source/SceneConstants.h"

namespace scene::source {

    struct Primitive {
        std::vector<DirectX::XMFLOAT3> positions;
        std::vector<DirectX::XMFLOAT3> normals;
        std::vector<DirectX::XMFLOAT4> tangents;
        std::vector<DirectX::XMFLOAT2> uv0;
        std::vector<DirectX::XMFLOAT2> uv1;
        std::vector<DirectX::XMFLOAT4> color0;
        std::vector<DirectX::XMFLOAT4> color1;
        std::vector<uint32_t> indices;

        uint32_t material_id = SceneConstants::INVALID_INDEX;

        void validate() const;
    };

    struct Mesh {
        std::string name;
        std::vector<Primitive> primitives;

        void validate() const;
    };
}
