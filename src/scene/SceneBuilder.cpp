#include "scene/SceneBuilder.h"

#include <random>
#include <algorithm>

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
				ret->indices.push_back(ind);
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
		std::normal_distribution<float> dist_rot{ 0.0f, 1.0f };

		ret->objects.reserve(info.object_count);
		for (uint32_t i = 0; i < info.object_count; ++i) {
			SceneDataCPU::Object obj{};
			obj.object_id = i;
			obj.material_index = dist_material(gen);
			obj.mesh_index = dist_mesh(gen);
			obj.flags = 0;

			DirectX::XMVECTOR q{ dist_rot(gen), dist_rot(gen) , dist_rot(gen) , dist_rot(gen) };
			const float length_2 = DirectX::XMVectorGetX(DirectX::XMQuaternionLengthSq(q));
			if (length_2 < 1e-12f) q = DirectX::XMQuaternionIdentity();
			q = DirectX::XMQuaternionNormalize(q);

			DirectX::XMMATRIX transform =
				DirectX::XMMatrixScaling(info.radius, info.radius, info.radius) *
				DirectX::XMMatrixRotationQuaternion(q) *
				DirectX::XMMatrixTranslation(dist_xy(gen), dist_xy(gen), dist_z(gen));
			DirectX::XMStoreFloat4x4(&obj.transform, DirectX::XMMatrixTranspose(transform));
			ret->objects.push_back(obj);
		}

		std::sort(ret->objects.begin(), ret->objects.end(),
			[](const scene::SceneDataCPU::Object& a, const scene::SceneDataCPU::Object& b)->bool {
				if (a.mesh_index != b.mesh_index) return a.mesh_index < b.mesh_index;
				if (a.material_index != b.material_index) return a.material_index < b.material_index;
				return a.object_id < b.object_id;
			});

		ret->build_mesh_aabbs_from_vertices();
		ret->build_batches_from_objects();

		ret->loaded = true;
		return ret;
	}

	std::unique_ptr<SceneDataCPU> SceneBuilder::build_squares(const SceneInfoSphere& info) {

		auto ret = std::make_unique<SceneDataCPU>();

		ret->source_path = "";

		std::mt19937 gen(info.seed);

		// gen material (duplicate same materials)

		util::Logger::g_logger.assert_with_log(info.material_count > 0, "material count must > 0");
		ret->build_random_material(info.material_count);

		// gen mesh (duplicate same meshes)

		util::Logger::g_logger.assert_with_log(info.mesh_count > 0, "mesh count must > 0");
		util::Logger::g_logger.assert_with_log(info.mesh_division > 0, "mesh division must > 0");

		ret->meshes.reserve(info.mesh_count);
		auto geom = GenedMesh::generate_fullquad(info.mesh_division);

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
			mesh.name = "quad_" + std::to_string(i);
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
				ret->indices.push_back(ind);
			}
			ret->meshes.push_back(mesh);
		}

		// gen object

		/*
			줄세워서 만들고
			필요에 따라 셔플하거나 역정렬한뒤에
			material / mesh 할당하고
			batch 를 만듬
		*/

		ret->objects.reserve(info.object_count);
		std::vector<uint32_t> indices;
		indices.reserve(info.object_count);

		// generate indices by overdraw
		{
			util::Logger::g_logger.assert_with_log(
				info.overdraw_count < std::numeric_limits<uint32_t>::max() - 1,
				"overdraw_count overflow");
			util::Logger::g_logger.assert_with_log(
				info.object_count >= info.overdraw_count + 1,
				"there must be object_count >= overdraw_count + 1");

			uint32_t i = 0;
			for (; i < info.overdraw_count + 1; ++i)
				indices.push_back(info.overdraw_count - i);
			for (; i < info.object_count; ++i)
				indices.push_back(i);
		}

		for (uint32_t i = 0; i < info.object_count; ++i) {
			uint32_t ind = indices[i];
			SceneDataCPU::Object obj{};
			obj.object_id = i;
			obj.material_index = 0;
			obj.mesh_index = 0;
			obj.flags = 0;
			
			float pos_z_add = 0.0f;
			if (i != 0 && info.to_remain_only_in_camera)
				pos_z_add = -1000.0f;

			DirectX::XMMATRIX transform =
				DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f) *
				DirectX::XMMatrixTranslation(0.0f, 0.0f,
					pos_z_add + static_cast<float>(ind) / static_cast<float>(info.object_count));
			DirectX::XMStoreFloat4x4(&obj.transform, DirectX::XMMatrixTranspose(transform));
			ret->objects.push_back(obj);
		}

		const uint32_t mesh_stride = std::max(1U, info.object_count / info.mesh_count);
		const uint32_t material_stride = std::max(1U, info.object_count / info.material_count);
		const uint32_t final_stride = std::min(mesh_stride, material_stride);

		uint32_t ind = 0;
		uint32_t mesh_ind = 0;
		uint32_t mat_ind = 0;
		for (uint32_t i = 0; i < info.object_count; ++i) {
			auto& obj = ret->objects[i];
			obj.mesh_index = mesh_ind;
			obj.material_index = mat_ind;
			if (++ind == final_stride) {
				ind = 0;
				if (++mesh_ind == info.mesh_count) mesh_ind = 0;
				if (++mat_ind == info.material_count) mat_ind = 0;
			}
		}

		ret->build_mesh_aabbs_from_vertices();
		ret->build_batches_from_objects();

		ret->loaded = true;
		return ret;

	}
}
