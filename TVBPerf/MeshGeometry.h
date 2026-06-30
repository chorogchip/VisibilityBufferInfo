#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <DirectXMath.h>

struct MeshGeometry {

	struct Vertex {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
	};

	struct SubMesh {
		uint32_t start_offset;
		uint32_t index_count;
		uint32_t material_index;
	};

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<SubMesh> submeshes;
	
	static MeshGeometry generate_triangle();
};

static_assert(sizeof(MeshGeometry::Vertex) == sizeof(float) * (3 + 3 + 2), "invalid size");