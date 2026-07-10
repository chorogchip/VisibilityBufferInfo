#include "Camera.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace rndr {
	static constexpr float PITCH_MAX = 85.0f * XM_PI / 180.0f;

	void Camera::move_pos(float dx, float dy, float dz) {
		position_.x += dx;
		position_.y += dy;
		position_.z += dz;
	}

	void Camera::set_pos(float x, float y, float z) {
		position_ = XMFLOAT3(x, y, z);
	}

	void Camera::lookat(float x, float y, float z) {
		float dx = x - position_.x;
		float dy = y - position_.y;
		float dz = z - position_.z;

		float dist_sq_xz = dx * dx + dz * dz;
		if (dist_sq_xz <= 0.0000001f) return;

		yaw_ = std::atan2(dx, dz);
		pitch_ = std::atan2(dy, std::sqrt(dist_sq_xz));

		yaw_ = std::remainder(yaw_, XM_2PI);
		pitch_ = std::clamp(pitch_, -PITCH_MAX, PITCH_MAX);
	}

	void Camera::turn_right(float yaw_radian) {
		yaw_ = std::remainder(yaw_ + yaw_radian, XM_2PI);
	}

	void Camera::turn_up(float pitch_radian) {
		pitch_ = std::clamp(pitch_ + pitch_radian, -PITCH_MAX, PITCH_MAX);
	}

	void Camera::move_forward(float distance) {
		float sin_yaw = 0.0f;
		float cos_yaw = 0.0f;
		XMScalarSinCos(&sin_yaw, &cos_yaw, yaw_);

		position_.x += sin_yaw * distance;
		position_.z += cos_yaw * distance;
	}

	void Camera::move_right(float distance) {
		float sin_yaw = 0.0f;
		float cos_yaw = 0.0f;
		XMScalarSinCos(&sin_yaw, &cos_yaw, yaw_);

		position_.x += cos_yaw * distance;
		position_.z -= sin_yaw * distance;
	}

	void Camera::set_fovy_nearz_farz(float fovy_radian, float near_z, float far_z) {
		fovy_ = fovy_radian;
		near_z_ = near_z;
		far_z_ = far_z;
	}

	XMMATRIX Camera::get_mat_view() const {
		XMVECTOR pos = XMLoadFloat3(&position_);

		float sin_yaw = 0.0f;
		float cos_yaw = 0.0f;
		float sin_pitch = 0.0f;
		float cos_pitch = 0.0f;
		XMScalarSinCos(&sin_yaw, &cos_yaw, yaw_);
		XMScalarSinCos(&sin_pitch, &cos_pitch, pitch_);

		XMVECTOR forward = XMVectorSet(sin_yaw * cos_pitch, sin_pitch, cos_yaw * cos_pitch, 0.0f);
		XMVECTOR target = XMVectorAdd(pos, forward);
		XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		return XMMatrixLookAtLH(pos, target, up);
	}

	XMMATRIX Camera::get_mat_proj(unsigned width, unsigned height) const {
		float aspect = 1.0f;
		if (width != 0 && height != 0) {
			aspect = static_cast<float>(width) / static_cast<float>(height);
		}
		return XMMatrixPerspectiveFovLH(fovy_, aspect, near_z_, far_z_);
	}
}