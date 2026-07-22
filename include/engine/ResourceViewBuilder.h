#pragma once

#include <d3d12.h>

namespace eng {
	class ResourceViewBuilder {

	public:
		enum class EnumResourceType {
			BUFFER,
			ARRAY_2D,
			CUBEMAP,
		};

		static D3D12_SHADER_RESOURCE_VIEW_DESC build_srv(
			ID3D12Resource* resource,
			EnumResourceType type = EnumResourceType::BUFFER,
			DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);

		static D3D12_UNORDERED_ACCESS_VIEW_DESC build_uav(
			ID3D12Resource* resource,
			EnumResourceType type = EnumResourceType::BUFFER,
			DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
	};
}