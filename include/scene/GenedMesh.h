#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <DirectXMath.h>

class GenedMesh {
private:
	GenedMesh() = default;

public:

	struct Vertex {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
	};

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	
	static GenedMesh generate_triangle();
	static GenedMesh generate_sphere(int division);
	static GenedMesh generate_fullquad(int division);
};

static_assert(sizeof(GenedMesh::Vertex) == sizeof(float) * (3 + 3 + 2), "invalid size");