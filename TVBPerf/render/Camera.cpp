#include "Camera.h"

#include <DirectXMath.h>
#include <cmath>

using namespace DirectX;

namespace rndr {

    void Camera::move_pos(float dx, float dy, float dz) {
        position_.x += dx;
        position_.y += dy;
        position_.z += dz;
    }

    void Camera::set_pos(float x, float y, float z) {
        position_ = XMFLOAT3(x, y, z);
    }

    void Camera::lookat(float x, float y, float z) {
        XMVECTOR pos = XMLoadFloat3(&position_);
        XMVECTOR target = XMVectorSet(x, y, z, 1.0f);

        XMVECTOR dir = XMVector3Normalize(target - pos);

        float dx = XMVectorGetX(dir);
        float dz = XMVectorGetZ(dir);

        yaw_ = XMConvertToDegrees(std::atan2f(dx, dz));
    }

    void Camera::turn(float yaw_degree) {
        yaw_ += yaw_degree;
    }

    void Camera::move_forward(float distance) {
        float yaw_rad = DirectX::XMConvertToRadians(yaw_);

        position_.x += std::sinf(yaw_rad) * distance;
        position_.z += std::cosf(yaw_rad) * distance;
    }

    void Camera::move_right(float distance) {
        float yaw_rad = DirectX::XMConvertToRadians(yaw_);

        position_.x += std::cosf(yaw_rad) * distance;
        position_.z -= std::sinf(yaw_rad) * distance;
    }
    
    void Camera::set_fovy_nearz_farz(float fovy, float near_z, float far_z) {
        fovy_ = fovy;
        near_z_ = near_z;
        far_z_ = far_z;
    }

    XMMATRIX Camera::get_mat_view() const {
        XMVECTOR pos = XMLoadFloat3(&position_);

        float yaw_rad = XMConvertToRadians(yaw_);

        XMVECTOR forward = XMVectorSet(std::sinf(yaw_rad), 0.0f, std::cosf(yaw_rad), 0.0f);

        XMVECTOR target = pos + forward;
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        return XMMatrixLookAtLH(pos, target, up);
    }

    XMMATRIX Camera::get_mat_proj(unsigned width, unsigned height) const {
        float aspect = 1.0f;
        if (height != 0) aspect = static_cast<float>(width) / static_cast<float>(height);
        return XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), aspect, 0.1f, 1000.0f);
    }

}