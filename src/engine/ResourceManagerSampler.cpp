#include "engine/ResourceManagerSampler.h"

#include "util/Logger.h"

namespace eng {

    void ResourceManagerSampler::init(ID3D12Device* device) {
        device_ = device;
        heap_.Reset();

        util::Logger::g_logger.assert_with_log(
            device_ != nullptr, "sampler descriptor manager requires a device");

        descriptor_size_ = device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        heap_desc.NumDescriptors = DESCRIPTOR_COUNT;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        const HRESULT result = device_->CreateDescriptorHeap(
            &heap_desc, IID_PPV_ARGS(heap_.ReleaseAndGetAddressOf()));
        util::Logger::g_logger.assert_with_log(
            SUCCEEDED(result), "create sampler descriptor heap");
    }

    void ResourceManagerSampler::create_sampler(
        EnumDescPos position,
        const D3D12_SAMPLER_DESC& desc) {
        const UINT index = static_cast<UINT>(position);
        util::Logger::g_logger.assert_with_log(
            index < DESCRIPTOR_COUNT, "invalid sampler descriptor request");
        device_->CreateSampler(&desc, get_cpu_adr(position));
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ResourceManagerSampler::get_cpu_adr(
        EnumDescPos position) const {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = heap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(static_cast<UINT>(position)) * descriptor_size_;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE ResourceManagerSampler::get_gpu_adr(
        EnumDescPos position) const {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = heap_->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<UINT64>(static_cast<UINT>(position)) * descriptor_size_;
        return handle;
    }

}
