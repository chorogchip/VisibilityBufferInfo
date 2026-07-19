#include "engine/ResourceManagerFrame.h"

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
    }

    void ResourceManagerFrame::init(ID3D12Device* device) {
        device_ = device;
        rtv_heap_.Reset();
        dsv_heap_.Reset();

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
        device_->CreateRenderTargetView(texture, nullptr, get_rtv(position));
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

        device_->CreateDepthStencilView(texture, &desc, get_dsv(position));
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ResourceManagerFrame::get_rtv(EnumRTV position) const {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(index(position)) * rtv_size_;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ResourceManagerFrame::get_dsv(EnumDSV position) const {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(index(position)) * dsv_size_;
        return handle;
    }

}
