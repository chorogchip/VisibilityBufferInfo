#include "scene/SceneBuilder.h"

#include <cassert>
#include <random>

#include "scene/GenedMesh.h"

namespace scene {

	std::unique_ptr<SceneDataCPU> SceneBuilder::build(const SceneInfoSphere& info) {
		std::unique_ptr<SceneDataCPU> ret{ new SceneDataCPU() };

		ret->source_path = "";

		std::mt19937 gen(info.seed);

		for (uint32_t i = 0; i < info.material_count; ++i) {
			ret->materials.push_back(SceneDataCPU::Material{});
		}

		assert(info.material_count > 0);
		std::uniform_int_distribution<uint32_t> dist_material{ 0, info.material_count - 1 };

		for (uint32_t i = 0; i < info.mesh_count; ++i) {
			auto geom = GenedMesh::generate_sphere(info.mesh_division);
			SceneDataCPU::Mesh mesh{};
			mesh.name = "some sphere";
			mesh.vertex_start = ret->vertices.size();
			mesh.vertex_count = geom.vertices.size();
			mesh.index_start = ret->indices.size();
			mesh.index_count = geom.indices.size();
			for (const auto& vtx : geom.vertices) {
				SceneDataCPU::Vertex vertex{};
				vertex.position = vtx.position;
				vertex.normal = vtx.normal;
				vertex.uv0 = vtx.uv;
				ret->vertices.push_back(vertex);
			}
			for (const auto& ind : geom.indices) {
				ret->indices.push_back(mesh.vertex_start + ind);
			}
			ret->meshes.push_back(mesh);
		}

		assert(info.mesh_count > 0);
		std::uniform_int_distribution<uint32_t> dist_mesh{ 0, info.mesh_count - 1 };

		std::uniform_real_distribution<float> dist_xy{ -info.xy_minmax, info.xy_minmax };
		std::uniform_real_distribution<float> dist_z{ info.z_min, info.z_max };

		assert(info.sphere_count > 0);
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
			info.radius;
			ret->objects.push_back(obj);
		}

		ret->loaded = true;
		return ret;
	}
}
