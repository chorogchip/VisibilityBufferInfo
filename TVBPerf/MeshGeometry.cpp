#include "MeshGeometry.h"

MeshGeometry MeshGeometry::generate_triangle() {
    MeshGeometry ret{};

    ret.vertices = {
        {
            DirectX::XMFLOAT3{  0.0f,  0.5f, 0.0f },
            DirectX::XMFLOAT3{  0.0f,  0.0f, -1.0f },
            DirectX::XMFLOAT2{  0.5f,  0.0f }
        },
        {
            DirectX::XMFLOAT3{  0.5f, -0.5f, 0.0f },
            DirectX::XMFLOAT3{  0.0f,  0.0f, -1.0f },
            DirectX::XMFLOAT2{  1.0f,  1.0f }
        },
        {
            DirectX::XMFLOAT3{ -0.5f, -0.5f, 0.0f },
            DirectX::XMFLOAT3{  0.0f,  0.0f, -1.0f },
            DirectX::XMFLOAT2{  0.0f,  1.0f }
        },
};

    ret.indices = { 0, 1, 2 };

    ret.submeshes = {
        { 0, 3, 0 },
    };

    return ret;
}