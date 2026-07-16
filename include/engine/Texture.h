#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <string>

namespace eng {

	struct Texture {
		Microsoft::WRL::ComPtr<ID3D12Resource> texture_base_or_diffuse;
	};
}