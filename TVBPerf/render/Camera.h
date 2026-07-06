#pragma once

#include <DirectXMath.h>

namespace rndr {
	class Camera {
	public:
		Camera() = default;
		void move_pos(float dx, float dy, float dz);
		void set_pos(float x, float y, float z);
		void lookat(float x, float y, float z);
		void turn(float yaw_degree);
		void move_forward(float distance);
		void move_right(float distance);

		void set_fovy_nearz_farz(float fovy, float near_z, float far_z);

		DirectX::XMMATRIX get_mat_view() const;
		DirectX::XMMATRIX get_mat_proj(unsigned width, unsigned height) const;

	private:
		DirectX::XMFLOAT3 position_{};
		float yaw_ = 0.0f;
		float fovy_ = 45.0f;
		float near_z_ = 0.1f, far_z_ = 1000.0f;
	};
}