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

    return ret;
}

MeshGeometry MeshGeometry::generate_sphere(int division_min, int division_max) {
    MeshGeometry ret{};

    // TODO

    return ret;
}