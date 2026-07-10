#include "GenedMesh.h"

#include <cassert>
#include <cmath>
#include <DirectXMath.h>

#include "util/Logger.h"

GenedMesh GenedMesh::generate_triangle() {
    GenedMesh ret{};

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

GenedMesh GenedMesh::generate_sphere(int division) {
    util::Logger::g_logger.assert_with_log(division > 0, "division must > 0");

    GenedMesh ret{};

    const float inv_division = 1.0f / static_cast<float>(division);

    using namespace DirectX;  // for XMVector

    XMVECTOR dirs[] = {
        XMVectorSet(1,  0,  0, 0),
        XMVectorSet(-1,  0,  0, 0),
        XMVectorSet(0,  1,  0, 0),
        XMVectorSet(0, -1,  0, 0),
        XMVectorSet(0,  0,  1, 0),
        XMVectorSet(0,  0, -1, 0),
    };

    int dir_ind_right[] = { 5, 4, 0, 0, 0, 1 };
    int dir_ind_up[] = { 2, 2, 5, 4, 2, 2 };

    const uint32_t row = static_cast<uint32_t>(division + 1);

    for (int i = 0; i < 6; ++i) {
        const uint32_t baseVertex = static_cast<uint32_t>(ret.vertices.size());

        const XMVECTOR dir = dirs[i];
        const XMVECTOR dir_right = dirs[dir_ind_right[i]];
        const XMVECTOR dir_up = dirs[dir_ind_up[i]];

        for (int ix = 0; ix <= division; ++ix) {
            const float fx = static_cast<float>(ix) * inv_division;
            const float sx = fx * 2.0f - 1.0f;

            for (int iy = 0; iy <= division; ++iy) {
                const float fy = static_cast<float>(iy) * inv_division;
                const float sy = fy * 2.0f - 1.0f;

                XMVECTOR cube_pos = dir + dir_right * sx + dir_up * sy;
                XMFLOAT3 p{}, p2{};
                XMStoreFloat3(&p, cube_pos);
                XMStoreFloat3(&p2, cube_pos * cube_pos);

                XMFLOAT3 sphere_pos{
                    p.x * std::sqrtf(1.0f - 0.5f * (p2.y + p2.z) + p2.y * p2.z / 3.0f),
                    p.y * std::sqrtf(1.0f - 0.5f * (p2.z + p2.x) + p2.z * p2.x / 3.0f),
                    p.z * std::sqrtf(1.0f - 0.5f * (p2.x + p2.y) + p2.x * p2.y / 3.0f)
                };

                XMVECTOR n = XMLoadFloat3(&sphere_pos);
                n = XMVector3Normalize(n);

                XMFLOAT3 normal;
                XMStoreFloat3(&normal, n);

                ret.vertices.emplace_back(Vertex{ sphere_pos, normal, XMFLOAT2{ fx, fy } });
            }
        }

        for (int ix = 0; ix < division; ++ix) {
            for (int iy = 0; iy < division; ++iy) {
                uint32_t v00 = baseVertex + ix * row + iy;
                uint32_t v10 = baseVertex + (ix + 1) * row + iy;
                uint32_t v01 = baseVertex + ix * row + (iy + 1);
                uint32_t v11 = baseVertex + (ix + 1) * row + (iy + 1);

                ret.indices.push_back(v00);
                ret.indices.push_back(v01);
                ret.indices.push_back(v10);

                ret.indices.push_back(v10);
                ret.indices.push_back(v01);
                ret.indices.push_back(v11);
            }
        }
    }

    return ret;
}