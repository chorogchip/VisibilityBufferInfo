#include "scene/SceneBuilder.h"

#include <random>

#include "util/Logger.h"
#include "scene/GenedMesh.h"

namespace scene {

	std::unique_ptr<SceneDataCPU> SceneBuilder::build(const SceneInfoSphere& info) {
		auto ret = std::make_unique<SceneDataCPU>();

		ret->source_path = "";

		std::mt19937 gen(info.seed);

		// gen material (duplicate same materials)

		util::Logger::g_logger.assert_with_log(info.material_count > 0, "material count must > 0");
		ret->build_random_material(info.material_count);
		std::uniform_int_distribution<uint32_t> dist_material{ 0, info.material_count - 1 };

		// gen mesh (duplicate same meshes)

		util::Logger::g_logger.assert_with_log(info.mesh_count > 0, "mesh count must > 0");
		util::Logger::g_logger.assert_with_log(info.mesh_division > 0, "mesh division must > 0");

		ret->meshes.reserve(info.mesh_count);
		auto geom = GenedMesh::generate_sphere(info.mesh_division);

		util::Logger::g_logger.assert_with_log_mul_overflow(
			sizeof(scene::SceneDataCPU::Vertex) * info.mesh_count, geom.vertices.size(), std::numeric_limits<uint32_t>::max(),
			"too much vertices on mesh"
		);
		util::Logger::g_logger.assert_with_log_mul_overflow(
			sizeof(scene::SceneDataCPU::Index) * info.mesh_count, geom.indices.size(), std::numeric_limits<uint32_t>::max(),
			"too much indices on mesh"
		);

		const size_t vertices_cnt = info.mesh_count * geom.vertices.size();
		const size_t indices_cnt = info.mesh_count * geom.indices.size();

		ret->vertices.reserve(vertices_cnt);
		ret->indices.reserve(indices_cnt);

		for (uint32_t i = 0; i < info.mesh_count; ++i) {
			SceneDataCPU::Mesh mesh{};
			mesh.name = "sphere_" + std::to_string(i);
			mesh.vertex_start = static_cast<uint32_t>(ret->vertices.size());
			mesh.vertex_count = static_cast<uint32_t>(geom.vertices.size());
			mesh.index_start = static_cast<uint32_t>(ret->indices.size());
			mesh.index_count = static_cast<uint32_t>(geom.indices.size());

			for (const auto& vtx : geom.vertices) {
				SceneDataCPU::Vertex vertex{};
				vertex.position = vtx.position;
				vertex.normal = vtx.normal;
				vertex.uv0 = vtx.uv;
				vertex.uv1 = vtx.uv;
				vertex.tangent = {};
				ret->vertices.push_back(vertex);
			}
			for (const auto& ind : geom.indices) {
				ret->indices.push_back(mesh.vertex_start + ind);
			}
			ret->meshes.push_back(mesh);
		}

		std::uniform_int_distribution<uint32_t> dist_mesh{ 0, info.mesh_count - 1 };

		// gen object

		util::Logger::g_logger.assert_with_log(info.xy_minmax > 0.0f, "xy_minmax must > 0");
		util::Logger::g_logger.assert_with_log(info.z_min < info.z_max, "there must be z_min < z_max");
		util::Logger::g_logger.assert_with_log(info.radius > 0.0f, "radius must > 0");
		std::uniform_real_distribution<float> dist_xy{ -info.xy_minmax, info.xy_minmax };
		std::uniform_real_distribution<float> dist_z{ info.z_min, info.z_max };

		ret->objects.reserve(info.sphere_count);
		for (uint32_t i = 0; i < info.sphere_count; ++i) {
			SceneDataCPU::Object obj{};
			obj.object_id = i;
			obj.material_index = dist_material(gen);
			obj.mesh_index = dist_mesh(gen);
			obj.flags = 0;
			DirectX::XMMATRIX transform =
				DirectX::XMMatrixScaling(info.radius, info.radius, info.radius) *
				DirectX::XMMatrixTranslation(dist_xy(gen), dist_xy(gen), dist_z(gen));
			DirectX::XMStoreFloat4x4(&obj.transform, DirectX::XMMatrixTranspose(transform));
			ret->objects.push_back(obj);
		}

		ret->build_batches_from_objects();

		ret->loaded = true;
		return ret;
	}
}
