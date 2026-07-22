#include "engine/ResourceViewBuilder.h"


namespace eng {

	D3D12_SHADER_RESOURCE_VIEW_DESC ResourceViewBuilder::build_srv(
		ID3D12Resource* resource,
		EnumResourceType type = EnumResourceType::BUFFER,
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN) {

		D3D12_SHADER_RESOURCE_VIEW_DESC ret{};


        const D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = resource_desc.Format;

        if (resource_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
            srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srv_desc.Buffer.NumElements = static_cast<UINT>(resource_desc.Width / sizeof(uint32_t));
            srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        } else if (resource_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
            resource_desc.DepthOrArraySize == 1) {
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = resource_desc.MipLevels;


        }
		return ret;
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC ResourceViewBuilder::build_uav(
		ID3D12Resource* resource,
		EnumResourceType type = EnumResourceType::BUFFER,
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN) {

		D3D12_UNORDERED_ACCESS_VIEW_DESC ret{};



        util::Logger::g_logger.assert_with_log(
            (resource->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0,
            "UAV resource was not created with unordered-access support");


		return ret;
	}
}




namespace {


    static DXGI_FORMAT resolve_view_format(
        const D3D12_RESOURCE_DESC& resource_desc,
        DXGI_FORMAT format) {

        return format == DXGI_FORMAT_UNKNOWN ? resource_desc.Format : format;
    }

    static void assure_texture_2d_resource(
        const D3D12_RESOURCE_DESC& resource_desc,
        const char* message) {

        util::Logger::g_logger.assert_with_log(
            resource_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
            resource_desc.SampleDesc.Count == 1,
            message);
    }

    static D3D12_UNORDERED_ACCESS_VIEW_DESC infer_uav_desc(ID3D12Resource* resource) {
        const D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
        uav_desc.Format = resource_desc.Format;
        if (resource_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
            uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uav_desc.Buffer.NumElements = static_cast<UINT>(
                resource_desc.Width / sizeof(uint32_t));
            uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        } else if (resource_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
            resource_desc.DepthOrArraySize == 1) {
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        }
        return uav_desc;
    }



    void ResourceManagerShader::create_srv(
        EnumDescPos position,
        ID3D12Resource* resource,
        const D3D12_SHADER_RESOURCE_VIEW_DESC* desc,
        UINT offset) {


        DescriptorRecord& record = descriptor_records_[index];
        if (record.is_initialized) {
            util::Logger::g_logger.assert_with_log(
                !record.is_uav &&
                record.resource == resource &&
                same_desc(record.srv_desc, *desc),
                "conflicting shader SRV descriptor request");
            return;
        }

        device_->CreateShaderResourceView(resource, desc, get_cpu_adr(position, offset));
        record.is_initialized = true;
        record.is_uav = false;
        record.resource = resource;
        record.srv_desc = *desc;
    }

    void ResourceManagerShader::create_srv_texture_2d(
        EnumDescPos position,
        ID3D12Resource* resource,
        DXGI_FORMAT format,
        UINT offset) {

        util::Logger::g_logger.assert_with_log(
            resource != nullptr, "invalid Texture2D SRV descriptor request");

        const D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
        assure_texture_2d_resource(
            resource_desc, "Texture2D SRV requires a non-MS Texture2D resource");
        util::Logger::g_logger.assert_with_log(
            resource_desc.DepthOrArraySize == 1,
            "Texture2D SRV requires one array slice");

        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Format = resolve_view_format(resource_desc, format);
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MostDetailedMip = 0;
        desc.Texture2D.MipLevels = resource_desc.MipLevels;
        desc.Texture2D.PlaneSlice = 0;
        desc.Texture2D.ResourceMinLODClamp = 0.f;

        this->create_srv(position, resource, &desc, offset);
    }

    void ResourceManagerShader::create_srv_texture_2d_array(
        EnumDescPos position,
        ID3D12Resource* resource,
        DXGI_FORMAT format,
        UINT offset) {

        util::Logger::g_logger.assert_with_log(
            resource != nullptr,
            "invalid Texture2DArray SRV descriptor request");

        const D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
        assure_texture_2d_resource(
            resource_desc,
            "Texture2DArray SRV requires a non-MS Texture2D resource");

        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Format = resolve_view_format(resource_desc, format);
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray.MostDetailedMip = 0;
        desc.Texture2DArray.MipLevels = resource_desc.MipLevels;
        desc.Texture2DArray.FirstArraySlice = 0;
        desc.Texture2DArray.ArraySize = resource_desc.DepthOrArraySize;
        desc.Texture2DArray.PlaneSlice = 0;
        desc.Texture2DArray.ResourceMinLODClamp = 0.f;

        this->create_srv(position, resource, &desc, offset);
    }

    void ResourceManagerShader::create_srv_texture_cube_array(
        EnumDescPos position,
        ID3D12Resource* resource,
        DXGI_FORMAT format,
        UINT offset) {

        util::Logger::g_logger.assert_with_log(
            resource != nullptr, "invalid TextureCubeArray SRV descriptor request");

        const D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
        assure_texture_2d_resource(
            resource_desc, "TextureCubeArray SRV requires a non-MS Texture2D resource");
        util::Logger::g_logger.assert_with_log(
            resource_desc.DepthOrArraySize >= 6 &&
            resource_desc.DepthOrArraySize % 6 == 0,
            "TextureCubeArray SRV requires a multiple of six array slices");

        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Format = resolve_view_format(resource_desc, format);
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
        desc.TextureCubeArray.MostDetailedMip = 0;
        desc.TextureCubeArray.MipLevels = resource_desc.MipLevels;
        desc.TextureCubeArray.First2DArrayFace = 0;
        desc.TextureCubeArray.NumCubes = resource_desc.DepthOrArraySize / 6;
        desc.TextureCubeArray.ResourceMinLODClamp = 0.f;

        this->create_srv(position, resource, &desc, offset);
    }

    void ResourceManagerShader::create_uav(
        EnumDescPos position,
        ID3D12Resource* resource,
        const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc,
        UINT offset) {

        const UINT index = static_cast<UINT>(position) + offset;
        util::Logger::g_logger.assert_with_log(
            resource != nullptr && index < descriptor_count_,
            "invalid shader UAV descriptor request");
        util::Logger::g_logger.assert_with_log(
            (resource->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0,
            "UAV resource was not created with unordered-access support");

        D3D12_UNORDERED_ACCESS_VIEW_DESC inferred{};
        if (!desc) {
            inferred = infer_uav_desc(resource);
            desc = &inferred;
        }

        DescriptorRecord& record = descriptor_records_[index];
        if (record.is_initialized) {
            util::Logger::g_logger.assert_with_log(
                record.is_uav &&
                record.resource == resource &&
                same_desc(record.uav_desc, *desc),
                "conflicting shader UAV descriptor request");
            return;
        }

        device_->CreateUnorderedAccessView(
            resource, nullptr, desc, get_cpu_adr(position, offset));
        record.is_initialized = true;
        record.is_uav = true;
        record.resource = resource;
        record.uav_desc = *desc;
    }

    void ResourceManagerShader::create_uav_texture_2d(
        EnumDescPos position,
        ID3D12Resource* resource,
        DXGI_FORMAT format,
        UINT offset) {

        util::Logger::g_logger.assert_with_log(
            resource != nullptr, "invalid Texture2D UAV descriptor request");

        const D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
        assure_texture_2d_resource(
            resource_desc, "Texture2D UAV requires a non-MS Texture2D resource");
        util::Logger::g_logger.assert_with_log(
            resource_desc.DepthOrArraySize == 1,
            "Texture2D UAV requires one array slice");

        D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
        desc.Format = resolve_view_format(resource_desc, format);
        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = 0;
        desc.Texture2D.PlaneSlice = 0;

        this->create_uav(position, resource, &desc, offset);
    }
}