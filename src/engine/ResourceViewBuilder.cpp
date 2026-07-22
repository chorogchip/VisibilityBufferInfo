#include "engine/ResourceViewBuilder.h"

#include <cstdint>

#include "util/Logger.h"

namespace {

    DXGI_FORMAT resolve_view_format(
        const D3D12_RESOURCE_DESC& resource_desc,
        DXGI_FORMAT requested_format) {

        return requested_format == DXGI_FORMAT_UNKNOWN
            ? resource_desc.Format
            : requested_format;
    }

    void assure_texture_2d_resource(
        const D3D12_RESOURCE_DESC& resource_desc,
        const char* message) {

        util::Logger::g_logger.assert_with_log(
            resource_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
            resource_desc.SampleDesc.Count == 1,
            message);
    }

    void assure_single_texture_2d_resource(
        const D3D12_RESOURCE_DESC& resource_desc,
        const char* message) {

        assure_texture_2d_resource(resource_desc, message);

        util::Logger::g_logger.assert_with_log(
            resource_desc.DepthOrArraySize == 1,
            message);
    }

    void assure_raw_buffer_resource(
        const D3D12_RESOURCE_DESC& resource_desc,
        DXGI_FORMAT requested_format,
        const char* message) {

        util::Logger::g_logger.assert_with_log(
            resource_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER,
            message);

        util::Logger::g_logger.assert_with_log(
            resource_desc.Width % sizeof(std::uint32_t) == 0,
            "RAW buffer size must be a multiple of four bytes");

        util::Logger::g_logger.assert_with_log(
            requested_format == DXGI_FORMAT_UNKNOWN ||
            requested_format == DXGI_FORMAT_R32_TYPELESS,
            "RAW buffer views only support DXGI_FORMAT_R32_TYPELESS");
    }

}

namespace eng {

    D3D12_SHADER_RESOURCE_VIEW_DESC ResourceViewBuilder::build_srv(
        ID3D12Resource* resource,
        EnumResourceType type,
        DXGI_FORMAT format) {

        util::Logger::g_logger.assert_with_log(
            resource != nullptr,
            "cannot build an SRV for a null resource");

        const D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();

        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        switch (type) {
        case EnumResourceType::BUFFER:
            assure_raw_buffer_resource(
                resource_desc,
                format,
                "buffer SRV requires a buffer resource");

            desc.Format = DXGI_FORMAT_R32_TYPELESS;
            desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = static_cast<UINT>(
                resource_desc.Width / sizeof(std::uint32_t));
            desc.Buffer.StructureByteStride = 0;
            desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            break;

        case EnumResourceType::TEXTURE_2D:
            assure_single_texture_2d_resource(
                resource_desc,
                "bad texture2d srv");

            desc.Format = resolve_view_format(resource_desc, format);
            desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

            desc.Texture2D.MostDetailedMip = 0;
            desc.Texture2D.MipLevels = resource_desc.MipLevels;
            desc.Texture2D.PlaneSlice = 0;
            desc.Texture2D.ResourceMinLODClamp = 0.0f;
            break;

        case EnumResourceType::ARRAY_2D:
            assure_texture_2d_resource(
                resource_desc,
                "Texture2DArray SRV requires a non-MS Texture2D resource");

            desc.Format = resolve_view_format(resource_desc, format);
            desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;

            desc.Texture2DArray.MostDetailedMip = 0;
            desc.Texture2DArray.MipLevels = resource_desc.MipLevels;
            desc.Texture2DArray.FirstArraySlice = 0;
            desc.Texture2DArray.ArraySize =
                static_cast<UINT>(resource_desc.DepthOrArraySize);
            desc.Texture2DArray.PlaneSlice = 0;
            desc.Texture2DArray.ResourceMinLODClamp = 0.0f;
            break;

        case EnumResourceType::CUBEMAP:
        case EnumResourceType::CUBEMAP_ARRAY:
            assure_texture_2d_resource(
                resource_desc,
                "cubemap SRV requires a non-MS Texture2D resource");

            util::Logger::g_logger.assert_with_log(
                resource_desc.DepthOrArraySize >= 6 &&
                resource_desc.DepthOrArraySize % 6 == 0,
                "cubemap SRV requires a multiple of six array slices");

            desc.Format = resolve_view_format(resource_desc, format);

            if (type == EnumResourceType::CUBEMAP &&
                resource_desc.DepthOrArraySize == 6) {
                desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;

                desc.TextureCube.MostDetailedMip = 0;
                desc.TextureCube.MipLevels = resource_desc.MipLevels;
                desc.TextureCube.ResourceMinLODClamp = 0.0f;
            } else {
                desc.ViewDimension =
                    D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;

                desc.TextureCubeArray.MostDetailedMip = 0;
                desc.TextureCubeArray.MipLevels = resource_desc.MipLevels;
                desc.TextureCubeArray.First2DArrayFace = 0;
                desc.TextureCubeArray.NumCubes =
                    static_cast<UINT>(resource_desc.DepthOrArraySize / 6);
                desc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
            }
            break;

        default:
            util::Logger::g_logger.assert_with_log(
                false,
                "unsupported SRV resource type");
            break;
        }

        return desc;
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC ResourceViewBuilder::build_uav(
        ID3D12Resource* resource,
        EnumResourceType type,
        DXGI_FORMAT format) {

        util::Logger::g_logger.assert_with_log(
            resource != nullptr,
            "cannot build a UAV for a null resource");

        const D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();

        util::Logger::g_logger.assert_with_log(
            (resource_desc.Flags &
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0,
            "UAV resource was not created with unordered-access support");

        D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};

        switch (type) {
        case EnumResourceType::BUFFER:
            assure_raw_buffer_resource(
                resource_desc,
                format,
                "buffer UAV requires a buffer resource");

            desc.Format = DXGI_FORMAT_R32_TYPELESS;
            desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = static_cast<UINT>(
                resource_desc.Width / sizeof(std::uint32_t));
            desc.Buffer.StructureByteStride = 0;
            desc.Buffer.CounterOffsetInBytes = 0;
            desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            break;

        case EnumResourceType::TEXTURE_2D:
            assure_single_texture_2d_resource(
                resource_desc,
                "bad texture2d uav");

            desc.Format = resolve_view_format(resource_desc, format);
            desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

            desc.Texture2D.MipSlice = 0;
            desc.Texture2D.PlaneSlice = 0;
            break;

        case EnumResourceType::ARRAY_2D:
            assure_texture_2d_resource(
                resource_desc,
                "Texture2DArray UAV requires a non-MS Texture2D resource");

            desc.Format = resolve_view_format(resource_desc, format);
            desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;

            desc.Texture2DArray.MipSlice = 0;
            desc.Texture2DArray.FirstArraySlice = 0;
            desc.Texture2DArray.ArraySize =
                static_cast<UINT>(resource_desc.DepthOrArraySize);
            desc.Texture2DArray.PlaneSlice = 0;
            break;

        case EnumResourceType::CUBEMAP:
        case EnumResourceType::CUBEMAP_ARRAY:
            assure_texture_2d_resource(
                resource_desc,
                "cubemap UAV requires a non-MS Texture2D resource");

            util::Logger::g_logger.assert_with_log(
                resource_desc.DepthOrArraySize >= 6 &&
                resource_desc.DepthOrArraySize % 6 == 0,
                "cubemap UAV requires a multiple of six array slices");

            desc.Format = resolve_view_format(resource_desc, format);
            desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;

            desc.Texture2DArray.MipSlice = 0;
            desc.Texture2DArray.FirstArraySlice = 0;
            desc.Texture2DArray.ArraySize =
                static_cast<UINT>(resource_desc.DepthOrArraySize);
            desc.Texture2DArray.PlaneSlice = 0;
            break;

        default:
            util::Logger::g_logger.assert_with_log(
                false,
                "unsupported UAV resource type");
            break;
        }

        return desc;
    }

}
