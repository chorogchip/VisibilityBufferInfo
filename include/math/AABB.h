#pragma once

#include <DirectXMath.h>

namespace math {

	struct AABB {
		bool is_valid = false;
		DirectX::XMFLOAT3 pos_min{};
		DirectX::XMFLOAT3 pos_max{};

		static AABB create_empty() noexcept;
		static AABB create_from_pos(const DirectX::XMFLOAT3& pos) noexcept;
		bool is_collide(const AABB& other) const noexcept;
		AABB get_union(const AABB& other) const noexcept;
		AABB get_transformed(const DirectX::XMFLOAT3X4& matrix) const noexcept;
		AABB get_transformed(const DirectX::XMFLOAT4X4& matrix) const noexcept;
	};

}
