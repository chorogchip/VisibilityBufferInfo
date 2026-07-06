#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <DirectXMath.h>

class MeshGeometry {
private:
	MeshGeometry() = default;

public:

	struct Vertex {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
	};

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	
	static MeshGeometry generate_triangle();
	static MeshGeometry generate_sphere(int division);
};

static_assert(sizeof(MeshGeometry::Vertex) == sizeof(float) * (3 + 3 + 2), "invalid size");