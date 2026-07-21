#include "engine/ResourceManagerSampler.h"

#include "util/Logger.h"

namespace eng {

    static D3D12_SAMPLER_DESC make_linear_sampler_desc(
        D3D12_TEXTURE_ADDRESS_MODE address_mode) {

        D3D12_SAMPLER_DESC sampler_desc{};
        sampler_desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = address_mode;
        sampler_desc.AddressV = address_mode;
        sampler_desc.AddressW = address_mode;
        sampler_desc.MaxAnisotropy = 1;
        sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler_desc.MinLOD = 0.f;
        sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;
        return sampler_desc;
    }

    static D3D12_SAMPLER_DESC make_linear_border_white_sampler_desc() {
        D3D12_SAMPLER_DESC sampler_desc = make_linear_sampler_desc(
            D3D12_TEXTURE_ADDRESS_MODE_BORDER);
        sampler_desc.BorderColor[0] = 1.f;
        sampler_desc.BorderColor[1] = 1.f;
        sampler_desc.BorderColor[2] = 1.f;
        sampler_desc.BorderColor[3] = 1.f;
        return sampler_desc;
    }

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
        EnumDescPos position) {

        this->create_sampler_linear_wrap(position);
    }

    void ResourceManagerSampler::create_sampler(
        EnumDescPos position,
        const D3D12_SAMPLER_DESC& desc) {

        const UINT index = static_cast<UINT>(position);
        util::Logger::g_logger.assert_with_log(
            index < DESCRIPTOR_COUNT, "invalid sampler descriptor request");

        device_->CreateSampler(&desc, get_cpu_adr(position));
    }

    void ResourceManagerSampler::create_sampler_linear_wrap(
        EnumDescPos position) {

        this->create_sampler(
            position, make_linear_sampler_desc(D3D12_TEXTURE_ADDRESS_MODE_WRAP));
    }

    void ResourceManagerSampler::create_sampler_linear_clamp(
        EnumDescPos position) {

        this->create_sampler(
            position, make_linear_sampler_desc(D3D12_TEXTURE_ADDRESS_MODE_CLAMP));
    }

    void ResourceManagerSampler::create_sampler_linear_border_white(
        EnumDescPos position) {

        this->create_sampler(position, make_linear_border_white_sampler_desc());
    }

    void ResourceManagerSampler::create_sampler_comparison_linear_border_white(
        EnumDescPos position) {

        D3D12_SAMPLER_DESC sampler_desc = make_linear_border_white_sampler_desc();
        sampler_desc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
        this->create_sampler(position, sampler_desc);
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
