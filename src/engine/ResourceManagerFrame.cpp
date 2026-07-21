#include "engine/ResourceManagerFrame.h"

#include <cstring>

#include "util/Logger.h"

namespace eng {

    namespace {
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> create_heap(
            ID3D12Device* device,
            D3D12_DESCRIPTOR_HEAP_TYPE type,
            UINT count) {
            D3D12_DESCRIPTOR_HEAP_DESC desc{};
            desc.Type = type;
            desc.NumDescriptors = count;
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
            const HRESULT result = device->CreateDescriptorHeap(
                &desc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf()));
            util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "create frame descriptor heap");
            return heap;
        }

        UINT index(ResourceManagerFrame::EnumRTV position) {
            return static_cast<UINT>(position);
        }

        UINT index(ResourceManagerFrame::EnumDSV position) {
            return static_cast<UINT>(position);
        }

        bool same_resource_desc(
            const D3D12_RESOURCE_DESC& lhs,
            const D3D12_RESOURCE_DESC& rhs) {
            return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
        }
    }

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
        rtv_heap_ = create_heap(device_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, RTV_COUNT);
        dsv_heap_ = create_heap(device_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, DSV_COUNT);
    }

    void ResourceManagerFrame::create_rtv(EnumRTV position, ID3D12Resource* texture) {
        const UINT descriptor_index = index(position);
        util::Logger::g_logger.assert_with_log(
            texture != nullptr && descriptor_index < RTV_COUNT,
            "invalid RTV descriptor request");

        const D3D12_RESOURCE_DESC resource_desc = texture->GetDesc();
        RtvRecord& record = rtv_records_[descriptor_index];
        if (record.initialized) {
            util::Logger::g_logger.assert_with_log(
                record.resource == texture &&
                same_resource_desc(record.resource_desc, resource_desc),
                "conflicting RTV descriptor request");
            return;
        }

        device_->CreateRenderTargetView(texture, nullptr, get_rtv(position));
        record.initialized = true;
        record.resource = texture;
        record.resource_desc = resource_desc;
    }

    void ResourceManagerFrame::create_dsv(EnumDSV position, ID3D12Resource* texture) {
        const UINT descriptor_index = index(position);
        util::Logger::g_logger.assert_with_log(
            texture != nullptr && descriptor_index < DSV_COUNT,
            "invalid DSV descriptor request");

        D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        if (position == EnumDSV::DEPTH_READ_ONLY)
            desc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;

        const D3D12_RESOURCE_DESC resource_desc = texture->GetDesc();
        DsvRecord& record = dsv_records_[descriptor_index];
        if (record.initialized) {
            util::Logger::g_logger.assert_with_log(
                record.resource == texture &&
                same_resource_desc(record.resource_desc, resource_desc) &&
                std::memcmp(&record.view_desc, &desc, sizeof(desc)) == 0,
                "conflicting DSV descriptor request");
            return;
        }

        device_->CreateDepthStencilView(texture, &desc, get_dsv(position));
        record.initialized = true;
        record.resource = texture;
        record.resource_desc = resource_desc;
        record.view_desc = desc;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ResourceManagerFrame::get_rtv(EnumRTV position, UINT offset) const {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(index(position) + offset) * rtv_size_;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ResourceManagerFrame::get_dsv(EnumDSV position, UINT offset) const {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(index(position) + offset) * dsv_size_;
        return handle;
    }

}
