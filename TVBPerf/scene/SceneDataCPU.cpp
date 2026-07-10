#include "SceneDataCPU.h"

#include "util/Logger.h"

namespace scene {

	void SceneDataCPU::build_random_material(size_t material_count) {
		this->materials.reserve(material_count);
		for (size_t i = 0; i < material_count; ++i) {
			this->materials.push_back(SceneDataCPU::Material{});
		}
	}

	void SceneDataCPU::build_batches_from_objects() {
		this->batches.clear();

		const size_t obj_cnt = objects.size();
		if (obj_cnt == 0) return;

		util::Logger::g_logger.assert_with_log(
			obj_cnt <= std::numeric_limits<uint32_t>::max(), "obj cnt over UINT_MAX");

		std::sort(this->objects.begin(), this->objects.end(),
			[](const scene::SceneDataCPU::Object& a, const scene::SceneDataCPU::Object& b)->bool {
				if (a.mesh_index != b.mesh_index) return a.mesh_index < b.mesh_index;
				if (a.material_index != b.material_index) return a.material_index < b.material_index;
				return a.object_id < b.object_id;
			});

		this->batches.push_back({ 0, 0 });
		size_t cur_size = 1;
		for (size_t i = 1; i < obj_cnt; ++i) {
			if (this->objects[i].mesh_index == this->objects[i - 1].mesh_index &&
				this->objects[i].material_index == this->objects[i - 1].material_index) {
				++cur_size;
			} else {
				this->batches.back().object_count = static_cast<uint32_t>(cur_size);
				this->batches.push_back({ static_cast<uint32_t>(i), 0 });
				cur_size = 1;
			}
		}
		this->batches.back().object_count = static_cast<uint32_t>(cur_size);
	}
}