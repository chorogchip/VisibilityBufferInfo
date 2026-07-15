#include "scene/SceneDataCPU.h"

#include "util/Logger.h"

namespace scene {


	void SceneDataCPU::build_random_material(size_t material_count) {

		this->materials.clear();
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

		ObjectBatch batch{};
		batch.object_index = 0;
		batch.object_count = 1;
		batch.material_index = objects[0].material_index;
		batch.mesh_index = objects[0].mesh_index;

		for (size_t i = 1; i < obj_cnt; ++i) {
			if (this->objects[i].material_index == batch.material_index &&
				this->objects[i].mesh_index == batch.mesh_index) {
				batch.object_count++;
			} else {
				this->batches.push_back(batch);
				batch.object_index = i;
				batch.object_count = 1;
				batch.material_index = this->objects[i].material_index;
				batch.mesh_index = this->objects[i].mesh_index;
			}
		}
		this->batches.push_back(batch);
	}
}
