#include "engine/ResourceManagerShader.h"

#include <algorithm>
#include <cstdint>

#include "util/Logger.h"

namespace eng {

    namespace {
        D3D12_SHADER_RESOURCE_VIEW_DESC infer_srv_desc(ID3D12Resource* resource) {
            const D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Format = resource_desc.Format;
            if (resource_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
                srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srv_desc.Buffer.NumElements = static_cast<UINT>(resource_desc.Width / sizeof(uint32_t));
                srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            }
            else if (resource_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && resource_desc.DepthOrArraySize == 1) {
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srv_desc.Texture2D.MipLevels = resource_desc.MipLevels;
            }
            return srv_desc;
        }
    }

    void ResourceManagerShader::init(ID3D12Device* device) {
        device_ = device;
        requested_count_ = 0;
        descriptor_size_ = 0;
        descriptor_count_ = 0;
        heap_.Reset();
        pending_descriptors_.clear();
    }

    void ResourceManagerShader::request(
        EnumDescPos position,
        ID3D12Resource* resource,
        const D3D12_SHADER_RESOURCE_VIEW_DESC* desc,
        UINT offset) {
        PendingDescriptor pending{};
        pending.index = static_cast<UINT>(position) + offset;
        pending.resource = resource;
        if (desc) {
            pending.desc = *desc;
            pending.has_desc = true;
        }
        pending_descriptors_.push_back(std::move(pending));
        requested_count_ = (std::max)(requested_count_, static_cast<UINT>(position) + offset + 1);
    }

    void ResourceManagerShader::build() {
        descriptor_count_ = requested_count_;
        descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_desc.NumDescriptors = descriptor_count_;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        const HRESULT result = device_->CreateDescriptorHeap(
            &heap_desc, IID_PPV_ARGS(heap_.ReleaseAndGetAddressOf()));
        util::Logger::g_logger.assert_with_log(
            SUCCEEDED(result), "create shader resource descriptor heap");

        for (const auto& pending : pending_descriptors_) {
            D3D12_SHADER_RESOURCE_VIEW_DESC inferred{};
            const D3D12_SHADER_RESOURCE_VIEW_DESC* desc = &pending.desc;
            if (!pending.has_desc) {
                inferred = infer_srv_desc(pending.resource.Get());
                desc = &inferred;
            }
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heap_->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += static_cast<SIZE_T>(pending.index) * descriptor_size_;
            device_->CreateShaderResourceView(pending.resource.Get(), desc, handle);
        }
    }

}
