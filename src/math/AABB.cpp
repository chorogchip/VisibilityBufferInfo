#include "math/AABB.h"

#include <algorithm>
#include <array>

namespace math {

	namespace {

		DirectX::XMFLOAT3 transform_point(
			const DirectX::XMFLOAT3& point,
			DirectX::FXMMATRIX matrix) noexcept {
			DirectX::XMFLOAT3 result;

			DirectX::XMStoreFloat3(
				&result,
				DirectX::XMVector3TransformCoord(
					DirectX::XMLoadFloat3(&point),
					matrix));

			return result;
		}

	}

	AABB AABB::create_empty() noexcept {
		return {};
	}

	AABB AABB::create_from_pos(const DirectX::XMFLOAT3& pos) noexcept {
		AABB result;
		result.is_valid = true;
		result.pos_min = pos;
		result.pos_max = pos;
		return result;
	}

	bool AABB::is_collide(const AABB& other) const noexcept {
		if (!is_valid || !other.is_valid) return false;

		return
			pos_min.x <= other.pos_max.x &&
			pos_max.x >= other.pos_min.x &&
			pos_min.y <= other.pos_max.y &&
			pos_max.y >= other.pos_min.y &&
			pos_min.z <= other.pos_max.z &&
			pos_max.z >= other.pos_min.z;
	}

	AABB AABB::get_union(const AABB& other) const noexcept {
		if (!is_valid) return other;
		if (!other.is_valid) return *this;

		AABB result;
		result.is_valid = true;

		result.pos_min = {
			std::min(pos_min.x, other.pos_min.x),
			std::min(pos_min.y, other.pos_min.y),
			std::min(pos_min.z, other.pos_min.z)
		};

		result.pos_max = {
			std::max(pos_max.x, other.pos_max.x),
			std::max(pos_max.y, other.pos_max.y),
			std::max(pos_max.z, other.pos_max.z)
		};

		return result;
	}

	AABB AABB::get_transformed(
		const DirectX::XMFLOAT3X4& matrix) const noexcept {
		if (!is_valid) return {};

		const DirectX::XMMATRIX transform =
			DirectX::XMLoadFloat3x4(&matrix);

		const std::array<DirectX::XMFLOAT3, 8> corners = {
			DirectX::XMFLOAT3{ pos_min.x, pos_min.y, pos_min.z },
			DirectX::XMFLOAT3{ pos_max.x, pos_min.y, pos_min.z },
			DirectX::XMFLOAT3{ pos_min.x, pos_max.y, pos_min.z },
			DirectX::XMFLOAT3{ pos_max.x, pos_max.y, pos_min.z },
			DirectX::XMFLOAT3{ pos_min.x, pos_min.y, pos_max.z },
			DirectX::XMFLOAT3{ pos_max.x, pos_min.y, pos_max.z },
			DirectX::XMFLOAT3{ pos_min.x, pos_max.y, pos_max.z },
			DirectX::XMFLOAT3{ pos_max.x, pos_max.y, pos_max.z }
		};

		AABB result =
			create_from_pos(transform_point(corners[0], transform));

		for (std::size_t i = 1; i < corners.size(); ++i) {
			const DirectX::XMFLOAT3 point =
				transform_point(corners[i], transform);

			result.pos_min.x = std::min(result.pos_min.x, point.x);
			result.pos_min.y = std::min(result.pos_min.y, point.y);
			result.pos_min.z = std::min(result.pos_min.z, point.z);

			result.pos_max.x = std::max(result.pos_max.x, point.x);
			result.pos_max.y = std::max(result.pos_max.y, point.y);
			result.pos_max.z = std::max(result.pos_max.z, point.z);
		}

		return result;
	}

	AABB AABB::get_transformed(
		const DirectX::XMFLOAT4X4& matrix) const noexcept {
		if (!is_valid) return {};

		const DirectX::XMMATRIX transform =
			DirectX::XMLoadFloat4x4(&matrix);

		const std::array<DirectX::XMFLOAT3, 8> corners = {
			DirectX::XMFLOAT3{ pos_min.x, pos_min.y, pos_min.z },
			DirectX::XMFLOAT3{ pos_max.x, pos_min.y, pos_min.z },
			DirectX::XMFLOAT3{ pos_min.x, pos_max.y, pos_min.z },
			DirectX::XMFLOAT3{ pos_max.x, pos_max.y, pos_min.z },
			DirectX::XMFLOAT3{ pos_min.x, pos_min.y, pos_max.z },
			DirectX::XMFLOAT3{ pos_max.x, pos_min.y, pos_max.z },
			DirectX::XMFLOAT3{ pos_min.x, pos_max.y, pos_max.z },
			DirectX::XMFLOAT3{ pos_max.x, pos_max.y, pos_max.z }
		};

		AABB result =
			create_from_pos(transform_point(corners[0], transform));

		for (std::size_t i = 1; i < corners.size(); ++i) {
			const DirectX::XMFLOAT3 point =
				transform_point(corners[i], transform);

			result.pos_min.x = std::min(result.pos_min.x, point.x);
			result.pos_min.y = std::min(result.pos_min.y, point.y);
			result.pos_min.z = std::min(result.pos_min.z, point.z);

			result.pos_max.x = std::max(result.pos_max.x, point.x);
			result.pos_max.y = std::max(result.pos_max.y, point.y);
			result.pos_max.z = std::max(result.pos_max.z, point.z);
		}

		return result;
	}

	DirectX::BoundingBox AABB::to_bounding_box() const noexcept {
		DirectX::XMFLOAT3 center{};
		DirectX::XMFLOAT3 extents{};

		center.x = (pos_min.x + pos_max.x) * 0.5f;
		center.y = (pos_min.y + pos_max.y) * 0.5f;
		center.z = (pos_min.z + pos_max.z) * 0.5f;

		extents.x = (pos_max.x - pos_min.x) * 0.5f;
		extents.y = (pos_max.y - pos_min.y) * 0.5f;
		extents.z = (pos_max.z - pos_min.z) * 0.5f;

		return DirectX::BoundingBox(center, extents);
	}

}
