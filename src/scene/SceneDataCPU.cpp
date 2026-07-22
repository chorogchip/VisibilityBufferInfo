#include "scene/SceneDataCPU.h"

#include <limits>

#include "util/Logger.h"

namespace scene {

	void SceneDataCPU::build_random_material(size_t material_count) {

		this->materials.clear();
		this->materials.reserve(material_count);
		for (size_t i = 0; i < material_count; ++i) {
			this->materials.push_back(eng::MaterialCPU{});
		}
	}

	void SceneDataCPU::build_mesh_aabbs_from_vertices() {
		for (auto& mesh : this->meshes) {
			mesh.local_aabb = math::AABB::create_empty();

			for (size_t i = mesh.vertex_start; i < mesh.vertex_start + mesh.vertex_count; ++i) {
				mesh.local_aabb = mesh.local_aabb.get_union(
					math::AABB::create_from_pos(this->vertices[i].position));
			}
		}
	}

	void SceneDataCPU::build_batches_from_objects() {
		this->all_batches.clear();
		this->batches.clear();

		const size_t obj_cnt = objects.size();
		if (obj_cnt == 0) return;

		util::Logger::g_logger.assert_with_log(
			obj_cnt <= std::numeric_limits<uint32_t>::max(),
			"obj cnt over UINT_MAX");

		ObjectBatch batch{};
		batch.object_index = 0;
		batch.object_count = 1;
		batch.material_index = objects[0].material_index;
		batch.mesh_index = objects[0].mesh_index;

		for (uint32_t i = 1; i < static_cast<uint32_t>(obj_cnt); ++i) {
			if (this->objects[i].material_index == batch.material_index &&
				this->objects[i].mesh_index == batch.mesh_index) {
				batch.object_count++;
			} else {
				this->all_batches.push_back(batch);
				batch.object_index = i;
				batch.object_count = 1;
				batch.material_index = this->objects[i].material_index;
				batch.mesh_index = this->objects[i].mesh_index;
			}
		}
		this->all_batches.push_back(batch);
		this->batches = this->all_batches;
	}

	void SceneDataCPU::build_batches_from_frustum(
		const DirectX::BoundingFrustum& frustum) {
		std::vector<uint8_t> is_inside_frustum_arr(this->objects.size(), 0);

		for (size_t i = 0; i < this->objects.size(); ++i) {
			const auto& object = this->objects[i];

			util::Logger::g_logger.assert_with_log(
				object.mesh_index < this->meshes.size(),
				"bad mesh index");

			const auto& mesh = this->meshes[object.mesh_index];
			if (!mesh.local_aabb.is_valid) continue;

			DirectX::XMFLOAT4X4 world_transform{};
			DirectX::XMStoreFloat4x4(
				&world_transform,
				DirectX::XMMatrixTranspose(
					DirectX::XMLoadFloat4x4(&object.transform)));

			const math::AABB world_aabb =
				mesh.local_aabb.get_transformed(world_transform);

			if (!world_aabb.is_valid) continue;

			is_inside_frustum_arr[i] =
				frustum.Intersects(world_aabb.to_bounding_box()) ? 1 : 0;
		}

		this->batches.clear();

		for (const auto& src_batch : this->all_batches) {
			const uint32_t begin = src_batch.object_index;
			const uint32_t end = begin + src_batch.object_count;

			util::Logger::g_logger.assert_with_log(
				begin <= end && end <= this->objects.size(),
				"bad batch range");

			ObjectBatch batch{};
			batch.material_index = src_batch.material_index;
			batch.mesh_index = src_batch.mesh_index;

			for (uint32_t i = begin; i < end; ++i) {
				if (!is_inside_frustum_arr[i]) {
					if (batch.object_count > 0) {
						this->batches.push_back(batch);
						batch.object_count = 0;
					}
					continue;
				}

				if (batch.object_count == 0) {
					batch.object_index = i;
					batch.object_count = 1;
				} else {
					batch.object_count++;
				}
			}

			if (batch.object_count > 0)
				this->batches.push_back(batch);
		}
	}

	void SceneDataCPU::reset_batches() {
		this->batches = this->all_batches;
	}

	uint64_t SceneDataCPU::count_batch_indices() const {
		uint64_t result = 0;

		for (const auto& batch : this->batches) {
			util::Logger::g_logger.assert_with_log(
				batch.mesh_index < this->meshes.size(),
				"bad mesh index");

			result += static_cast<uint64_t>(this->meshes[batch.mesh_index].index_count) *
				static_cast<uint64_t>(batch.object_count);
		}

		return result;
	}
}
