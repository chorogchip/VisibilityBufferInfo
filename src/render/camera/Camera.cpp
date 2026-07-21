#include "render/camera/Camera.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace rndr {

	static constexpr float PITCH_MAX = 85.0f * XM_PI / 180.0f;

	static XMVECTOR rotation_from_yaw_pitch(float yaw, float pitch) {
		return XMQuaternionRotationRollPitchYaw(pitch, yaw, 0.0f);
	}

	static void store_rotation(XMFLOAT4& rotation, float yaw, float pitch) {
		XMStoreFloat4(&rotation, XMQuaternionNormalize(rotation_from_yaw_pitch(yaw, pitch)));
	}

	static void get_yaw_pitch(const XMFLOAT4& rotation, float& yaw, float& pitch) {
		const XMVECTOR forward = XMVector3Rotate(
			XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMLoadFloat4(&rotation));
		XMFLOAT3 direction{};
		XMStoreFloat3(&direction, forward);
		yaw = std::atan2(direction.x, direction.z);
		pitch = std::asin(std::clamp(direction.y, -1.0f, 1.0f));
	}



	void Camera::init(const util::ProgramArgument& arg) {
		this->set_pos(
			arg.camera_pos_x,
			arg.camera_pos_y,
			arg.camera_pos_z);

		this->lookat(
			arg.camera_lookat_x,
			arg.camera_lookat_y,
			arg.camera_lookat_z);

		this->set_fovy_nearz_farz(
			arg.camera_fov,
			arg.camera_near_z,
			arg.camera_far_z);
	}

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

		float yaw = std::atan2(dx, dz);
		float pitch = std::atan2(dy, std::sqrt(dist_sq_xz));

		yaw = std::remainder(yaw, XM_2PI);
		pitch = std::clamp(pitch, -PITCH_MAX, PITCH_MAX);
		store_rotation(rotation_, yaw, pitch);
	}

	void Camera::turn_right(float yaw_radian) {
		float yaw = 0.0f, pitch = 0.0f;
		get_yaw_pitch(rotation_, yaw, pitch);
		yaw = std::remainder(yaw + yaw_radian, XM_2PI);
		store_rotation(rotation_, yaw, pitch);
	}

	void Camera::turn_up(float pitch_radian) {
		float yaw = 0.0f, pitch = 0.0f;
		get_yaw_pitch(rotation_, yaw, pitch);
		pitch = std::clamp(pitch + pitch_radian, -PITCH_MAX, PITCH_MAX);
		store_rotation(rotation_, yaw, pitch);
	}

	void Camera::move_forward(float distance) {
		float yaw = 0.0f, pitch = 0.0f;
		get_yaw_pitch(rotation_, yaw, pitch);
		float sin_yaw = 0.0f;
		float cos_yaw = 0.0f;
		XMScalarSinCos(&sin_yaw, &cos_yaw, yaw);

		position_.x += sin_yaw * distance;
		position_.z += cos_yaw * distance;
	}

	void Camera::move_right(float distance) {
		float yaw = 0.0f, pitch = 0.0f;
		get_yaw_pitch(rotation_, yaw, pitch);
		float sin_yaw = 0.0f;
		float cos_yaw = 0.0f;
		XMScalarSinCos(&sin_yaw, &cos_yaw, yaw);

		position_.x += cos_yaw * distance;
		position_.z -= sin_yaw * distance;
	}

	void Camera::set_fovy_nearz_farz(float fovy_radian, float near_z, float far_z) {
		fovy_ = fovy_radian;
		near_z_ = near_z;
		far_z_ = far_z;
	}

	CameraPose Camera::get_pose() const {
		return CameraPose{ position_, rotation_ };
	}

	void Camera::set_pose(const CameraPose& pose) {
		position_ = pose.position;
		XMVECTOR rotation = XMLoadFloat4(&pose.rotation);
		const float length_sq = XMVectorGetX(XMVector4LengthSq(rotation));
		if (length_sq <= 0.0000001f) rotation = XMQuaternionIdentity();
		rotation = XMQuaternionNormalize(rotation);
		XMStoreFloat4(&rotation_, rotation);

	}

	XMMATRIX Camera::get_mat_view() const {
		XMVECTOR pos = XMLoadFloat3(&position_);

		XMVECTOR rotation = XMQuaternionNormalize(XMLoadFloat4(&rotation_));
		XMVECTOR forward = XMVector3Rotate(
			XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotation);
		XMVECTOR up = XMVector3Rotate(
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotation);

		XMVECTOR target = XMVectorAdd(pos, forward);
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
