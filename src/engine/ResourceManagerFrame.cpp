#include "engine/ResourceManagerFrame.h"

#include <cstring>

#include "util/Assume.h"
#include "util/Logger.h"
#include "util/Utils.h"

namespace eng {

    void ResourceManagerFrame::init(ID3D12Device* device) {
        device_ = device;
        rtv_heap_.Reset();
        dsv_heap_.Reset();
        rtv_records_ = {};
        dsv_records_ = {};

        util::Logger::g_logger.assert_with_log(
            device_ != nullptr, "frame descriptor manager requires a device");

        rtv_size_ = device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        dsv_size_ = device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        D3D12_DESCRIPTOR_HEAP_DESC desc_rtv{};
        desc_rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc_rtv.NumDescriptors = RTV_COUNT;
        util::Utils::throw_if_failed(device->CreateDescriptorHeap(
            &desc_rtv, IID_PPV_ARGS(rtv_heap_.ReleaseAndGetAddressOf())));

        D3D12_DESCRIPTOR_HEAP_DESC desc_dsv{};
        desc_rtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc_rtv.NumDescriptors = DSV_COUNT;
        util::Utils::throw_if_failed(device->CreateDescriptorHeap(
            &desc_rtv, IID_PPV_ARGS(dsv_heap_.ReleaseAndGetAddressOf())));
    }

    void ResourceManagerFrame::create_rtv(EnumRTV position, ID3D12Resource* texture) {

        const UINT descriptor_index = static_cast<UINT>(position);

        util::Logger::g_logger.assert_with_log(
            texture != nullptr &&
            descriptor_index < RTV_COUNT,
            "invalid RTV descriptor request");

        MSVC_ASSUME(descriptor_index < RTV_COUNT);

        const D3D12_RESOURCE_DESC resource_desc = texture->GetDesc();
        RtvRecord& record = rtv_records_[descriptor_index];

        if (record.is_initialized) {
            util::Logger::g_logger.assert_with_log(
                record.resource == texture &&
                std::memcmp(&record.resource_desc, &resource_desc, sizeof(resource_desc)) == 0,
                "conflicting RTV descriptor request");
        } else {
            device_->CreateRenderTargetView(texture, nullptr, get_rtv(position));
            record.is_initialized = true;
            record.resource = texture;
            record.resource_desc = resource_desc;
        }
    }

    void ResourceManagerFrame::create_dsv(EnumDSV position, ID3D12Resource* texture) {

        const UINT descriptor_index = static_cast<UINT>(position);

        util::Logger::g_logger.assert_with_log(
            texture != nullptr &&
            descriptor_index < DSV_COUNT,
            "invalid DSV descriptor request");

        D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        if (position == EnumDSV::DEPTH_READ_ONLY)
            desc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;

        const D3D12_RESOURCE_DESC resource_desc = texture->GetDesc();
        DsvRecord& record = dsv_records_[descriptor_index];

        if (record.is_initialized) {
            util::Logger::g_logger.assert_with_log(
                true || // temp
                record.resource == texture &&
                std::memcmp(&record.resource_desc, &resource_desc, sizeof(resource_desc)) == 0 &&
                std::memcmp(&record.view_desc, &desc, sizeof(desc)) == 0,
                "conflicting DSV descriptor request");
        } else {
            device_->CreateDepthStencilView(texture, &desc, get_dsv(position));
            record.is_initialized = true;
            record.resource = texture;
            record.resource_desc = resource_desc;
            record.view_desc = desc;
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ResourceManagerFrame::get_rtv(EnumRTV position, UINT offset) const {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(static_cast<UINT>(position) + offset) * rtv_size_;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ResourceManagerFrame::get_dsv(EnumDSV position, UINT offset) const {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(static_cast<UINT>(position) + offset) * dsv_size_;
        return handle;
    }

}