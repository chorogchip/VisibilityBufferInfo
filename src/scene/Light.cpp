#include "scene/Lights.h"

#include <assimp/scene.h>

namespace scene {

	std::vector<Light> Light::from_assimp(
		const void* lights, size_t counts) {

		std::vector<Light> ret;

		for (size_t i = 0; i < counts; ++i) {
			const aiLight& src = *(const aiLight*)lights;

			Light light{};
			light.name = src.mName.C_Str();
			light.type =
				src.mType == aiLightSourceType::aiLightSource_UNDEFINED ? EnumLight::UNDEFINED :
				src.mType == aiLightSourceType::aiLightSource_DIRECTIONAL ? EnumLight::DIRECTIONAL :
				src.mType == aiLightSourceType::aiLightSource_POINT ? EnumLight::POINT :
				src.mType == aiLightSourceType::aiLightSource_SPOT ? EnumLight::SPOT :
				src.mType == aiLightSourceType::aiLightSource_AMBIENT ? EnumLight::AMBIENT :
				src.mType == aiLightSourceType::aiLightSource_AREA ? EnumLight::AREA :
				EnumLight::UNDEFINED;
			light.position = DirectX::XMFLOAT3(src.mPosition.x, src.mPosition.y, src.mPosition.z);
			light.direction = DirectX::XMFLOAT3(src.mDirection.x, src.mDirection.y, src.mDirection.z);
			light.up = DirectX::XMFLOAT3(src.mUp.x, src.mUp.y, src.mUp.z);
			light.attenuation_constant = src.mAttenuationConstant;
			light.attenuation_linear = src.mAttenuationLinear;
			light.attenuation_quadratic = src.mAttenuationQuadratic;
			light.color_diffuse = DirectX::XMFLOAT3(src.mColorDiffuse.r, src.mColorDiffuse.g, src.mColorDiffuse.b);
			light.color_specular = DirectX::XMFLOAT3(src.mColorSpecular.r, src.mColorSpecular.g, src.mColorSpecular.b);
			light.color_ambient = DirectX::XMFLOAT3(src.mColorAmbient.r, src.mColorAmbient.g, src.mColorAmbient.b);
			light.angle_inner_cone = src.mAngleInnerCone;
			light.angle_outer_cone = src.mAngleOuterCone;
			light.area = DirectX::XMFLOAT2(src.mSize.x, src.mSize.y);
		}
		return ret;
	}
}