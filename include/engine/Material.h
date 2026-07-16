#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <string>

namespace eng {


	struct MaterialConstant {

	};

	struct Material {
		uint32_t id;
		std::string name;
		Microsoft::WRL::ComPtr<ID3D12Resource> texture_base_or_diffuse;
		Microsoft::WRL::ComPtr<ID3D12Resource> texture_metalrough_or_specular;
		Microsoft::WRL::ComPtr<ID3D12Resource> texture_normal;
		Microsoft::WRL::ComPtr<ID3D12Resource> texture_emissive;
		Microsoft::WRL::ComPtr<ID3D12Resource> texture_occlusion;
		Microsoft::WRL::ComPtr<ID3D12Resource> texture_transmission;
		Microsoft::WRL::ComPtr<ID3D12Resource> texture_opacity;
		
		Microsoft::WRL::ComPtr<ID3D12Resource> buffer_constants;
		DirectX::XMFLOAT3 color_base_or_diffuse;
		DirectX::XMFLOAT3 color_specular;
		DirectX::XMFLOAT3 color_emissive;
		float val_emissive_intensity = 1.0f;
		float val_metalness = 0.0f;
		float val_roughness = 0.0f;
		float val_opacity = 1.0f;
		float val_alphacutoff = 0.5f;
		float val_transmission_factor = 1.0f;
		float val_normal_texture_scale = 1.0f;
		float val_occlusion_strength = 1.0f;
		DirectX::XMFLOAT2 normal_texture_transform_scale;

		bool enable_specular_gloss = false;
		bool enable_tex_base_or_diffuse = true;
		bool enable_tex_metalrough_or_specular = true;
		bool enable_tex_normal = true;
		bool enable_tex_emissive = true;
		bool enable_tex_occlusion = true;
		bool enable_tex_transmission = true;
		bool enable_tex_opacity = true;
	};
}