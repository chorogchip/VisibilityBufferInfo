#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <DirectXMath.h>

#include "engine/GPUResource.h"

namespace scene {

	struct TextureCPU {
		std::string name;
		std::filesystem::path path;

		[[nodiscard]] bool is_valid() const noexcept { return !path.empty(); }
	};

	struct MaterialCPU {
		std::string name;
		DirectX::XMFLOAT3 color_base{ 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT3 color_emissive{ 0.0f, 0.0f, 0.0f };

		TextureCPU tex_base_color;
		TextureCPU tex_metal_roughness;
		TextureCPU tex_normal;
		TextureCPU tex_emissive;
		TextureCPU tex_occlusion;

		static std::vector<MaterialCPU> from_assimp(
			void* materials, size_t material_cnt,
			const std::filesystem::path& scene_path);
	};

	struct MaterialGPU {
		eng::GPUResource tex_base_color;
		eng::GPUResource tex_metal_roughness;
		eng::GPUResource tex_normal;
		eng::GPUResource tex_emissive;
		eng::GPUResource tex_occlusion;
	};
}