#include "engine/ResourceManagerFrame.h"

#include "dx_util/DescriptorUtils.h"

namespace eng {

    namespace {
        UINT index(ResourceManagerFrame::EnumRTV position) {
            return static_cast<UINT>(position);
        }

        UINT index(ResourceManagerFrame::EnumDSV position) {
            return static_cast<UINT>(position);
        }
    }

    void ResourceManagerFrame::init(ID3D12Device* device) {
        device_ = device;
        rtv_indices_.fill(INVALID_INDEX);
        dsv_indices_.fill(INVALID_INDEX);
        for (auto& resource : rtv_resources_)
            resource.Reset();
        for (auto& resource : dsv_resources_)
            resource.Reset();
        rtv_heap_.Reset();
        dsv_heap_.Reset();
    }

    void ResourceManagerFrame::request_rtv(EnumRTV position, ID3D12Resource* texture) {
        rtv_resources_[index(position)] = texture;
    }

    void ResourceManagerFrame::request_dsv(EnumDSV position, ID3D12Resource* texture) {
        dsv_resources_[index(position)] = texture;
    }

    void ResourceManagerFrame::build() {
        UINT rtv_count = 0;
        UINT dsv_count = 0;
        for (UINT i = 0; i < RTV_COUNT; ++i) {
            if (rtv_resources_[i]) {
                rtv_indices_[i] = rtv_count;
                ++rtv_count;
            }
        }
        for (UINT i = 0; i < DSV_COUNT; ++i) {
            if (dsv_resources_[i]) {
                dsv_indices_[i] = dsv_count;
                ++dsv_count;
            }
        }

        rtv_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        dsv_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        if (rtv_count > 0)
            rtv_heap_ = dxutl::create_descriptor_heap(device_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, rtv_count, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
        if (dsv_count > 0)
            dsv_heap_ = dxutl::create_descriptor_heap(device_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, dsv_count, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

        for (UINT i = 0; i < RTV_COUNT; ++i) {
            if (rtv_resources_[i])
                device_->CreateRenderTargetView(rtv_resources_[i].Get(), nullptr, get_rtv(static_cast<EnumRTV>(i)));
        }
        for (UINT i = 0; i < DSV_COUNT; ++i) {
            if (!dsv_resources_[i])
                continue;

            D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_D32_FLOAT;
            desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            if (static_cast<EnumDSV>(i) == EnumDSV::DEPTH_READ_ONLY)
                desc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;

            device_->CreateDepthStencilView(
                dsv_resources_[i].Get(),
                &desc,
                get_dsv(static_cast<EnumDSV>(i)));
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ResourceManagerFrame::get_rtv(EnumRTV position) const {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(rtv_indices_[index(position)]) * rtv_size_;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ResourceManagerFrame::get_dsv(EnumDSV position) const {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(dsv_indices_[index(position)]) * dsv_size_;
        return handle;
    }

}
