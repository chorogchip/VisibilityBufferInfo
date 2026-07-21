#pragma once

#include <string>
#include <vector>
#include <DirectXMath.h>

namespace scene {

	enum class EnumLight {
		UNDEFINED,
		DIRECTIONAL,
		POINT,
		SPOT,
		AMBIENT,
		AREA,
	};

	struct Light {
		std::string name;
		EnumLight type;
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 direction;
		DirectX::XMFLOAT3 up;
		// Atten = 1/( att_c + att_l * d + att_q * d*d)
		float attenuation_constant;
		float attenuation_linear;
		float attenuation_quadratic;
		DirectX::XMFLOAT3 color_diffuse;
		DirectX::XMFLOAT3 color_specular;
		DirectX::XMFLOAT3 color_ambient;
		float angle_inner_cone;
		float angle_outer_cone;
		DirectX::XMFLOAT2 area;

		static std::vector<Light> from_assimp(
			const void* lights, size_t counts);
	};
}