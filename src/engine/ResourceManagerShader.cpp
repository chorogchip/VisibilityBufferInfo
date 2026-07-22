#include "engine/ResourceManagerShader.h"

#include <cstdint>
#include <cstring>

#include "util/Logger.h"
#include "util/Utils.h"

namespace eng {

    void ResourceManagerShader::init(ID3D12Device* device, UINT descriptor_count) {
        device_ = device;
        descriptor_count_ = descriptor_count;
        descriptor_records_.assign(descriptor_count_, {});
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

        util::Utils::throw_if_failed(device_->CreateDescriptorHeap(
            &heap_desc, IID_PPV_ARGS(heap_.ReleaseAndGetAddressOf())));
    }

    void ResourceManagerShader::create_srv(
        ID3D12Resource* resource,
        const D3D12_SHADER_RESOURCE_VIEW_DESC& desc,
        EnumDescPos position,
        UINT offset) {

        const UINT index = static_cast<UINT>(position) + offset;

        util::Logger::g_logger.assert_with_log(
            resource != nullptr && index < descriptor_count_,
            "invalid shader SRV descriptor request");

        DescriptorRecord& record = descriptor_records_[index];
        if (record.is_initialized) {
            util::Logger::g_logger.assert_with_log(
                !record.is_uav &&
                record.resource == resource &&
                std::memcmp(&record.srv_desc, &desc, sizeof(desc)) == 0,
                "conflicting shader SRV descriptor request");
            return;
        }

        device_->CreateShaderResourceView(resource, &desc, get_cpu_adr(position, offset));
        record.is_initialized = true;
        record.is_uav = false;
        record.resource = resource;
        record.srv_desc = desc;
    }

    void ResourceManagerShader::create_uav(
        ID3D12Resource* resource,
        const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc,
        EnumDescPos position,
        UINT offset) {

        const UINT index = static_cast<UINT>(position) + offset;

        util::Logger::g_logger.assert_with_log(
            resource != nullptr && index < descriptor_count_,
            "invalid shader UAV descriptor request");

        DescriptorRecord& record = descriptor_records_[index];
        if (record.is_initialized) {
            util::Logger::g_logger.assert_with_log(
                record.is_uav &&
                record.resource == resource &&
                std::memcmp(&record.uav_desc, &desc, sizeof(desc)) == 0,
                "conflicting shader UAV descriptor request");
            return;
        }

        device_->CreateUnorderedAccessView(resource, nullptr, &desc, get_cpu_adr(position, offset));
        record.is_initialized = true;
        record.is_uav = true;
        record.resource = resource;
        record.uav_desc = desc;
    }
}
