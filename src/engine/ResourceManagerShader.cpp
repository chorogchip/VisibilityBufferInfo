#include "engine/ResourceManagerShader.h"

#include <cstdint>

#include "util/Logger.h"

namespace eng {

    static D3D12_SHADER_RESOURCE_VIEW_DESC infer_srv_desc(ID3D12Resource* resource) {
        const D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = resource_desc.Format;
        if (resource_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
            srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srv_desc.Buffer.NumElements = static_cast<UINT>(resource_desc.Width / sizeof(uint32_t));
            srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        } else if (resource_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && resource_desc.DepthOrArraySize == 1) {
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = resource_desc.MipLevels;
        }
        return srv_desc;
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

    void ResourceManagerShader::init(ID3D12Device* device, UINT descriptor_count) {
        device_ = device;
        descriptor_count_ = descriptor_count;
        heap_.Reset();

        util::Logger::g_logger.assert_with_log(
            device_ != nullptr && descriptor_count_ > 0,
            "shader descriptor manager requires a device and non-zero capacity");

        descriptor_size_ = device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_desc.NumDescriptors = descriptor_count_;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        const HRESULT result = device_->CreateDescriptorHeap(
            &heap_desc, IID_PPV_ARGS(heap_.ReleaseAndGetAddressOf()));
        util::Logger::g_logger.assert_with_log(
            SUCCEEDED(result), "create shader resource descriptor heap");
    }

    void ResourceManagerShader::create_srv(
        EnumDescPos position,
        ID3D12Resource* resource,
        const D3D12_SHADER_RESOURCE_VIEW_DESC* desc,
        UINT offset) {

        const UINT index = static_cast<UINT>(position) + offset;
        util::Logger::g_logger.assert_with_log(
            resource != nullptr && index < descriptor_count_,
            "invalid shader SRV descriptor request");

        D3D12_SHADER_RESOURCE_VIEW_DESC inferred{};
        if (!desc) {
            inferred = infer_srv_desc(resource);
            desc = &inferred;
        }
        device_->CreateShaderResourceView(resource, desc, get_cpu_adr(position, offset));
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
        device_->CreateUnorderedAccessView(
            resource, nullptr, desc, get_cpu_adr(position, offset));
    }

}
