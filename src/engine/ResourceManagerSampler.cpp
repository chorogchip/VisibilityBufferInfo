#include "engine/ResourceManagerSampler.h"

#include "util/Logger.h"
#include "util/Utils.h"

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

        util::Utils::throw_if_failed(device_->CreateDescriptorHeap(
            &heap_desc, IID_PPV_ARGS(heap_.ReleaseAndGetAddressOf())));
    }

    void ResourceManagerSampler::create_sampler(
        EnumDescPos position,
        EnumSamplerType type) {

        D3D12_SAMPLER_DESC desc{};
        desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.MipLODBias = 0.0f;
        desc.MaxAnisotropy = 1;
        desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        desc.MinLOD = 0.0f;
        desc.MaxLOD = D3D12_FLOAT32_MAX;

        switch (type) {

        case EnumSamplerType::LINEAR_WRAP:
            break;
        case EnumSamplerType::LINEAR_CLAMP:
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            break;
        case EnumSamplerType::LINEAR_BORDER_WHITE:
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            desc.BorderColor[0] = 1.0f;
            desc.BorderColor[1] = 1.0f;
            desc.BorderColor[2] = 1.0f;
            desc.BorderColor[3] = 1.0f;
            break;
        case EnumSamplerType::LINEAR_BORDER_WHITE_COMP:
            desc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
            desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            desc.BorderColor[0] = 1.0f;
            desc.BorderColor[1] = 1.0f;
            desc.BorderColor[2] = 1.0f;
            desc.BorderColor[3] = 1.0f;
            break;
        default:
            util::Logger::g_logger.assert_with_log(
                false, "invalid sampler type");
        }

        this->create_sampler(position, desc);
    }

    void ResourceManagerSampler::create_sampler(
        EnumDescPos position,
        const D3D12_SAMPLER_DESC& desc) {

        const UINT index = static_cast<UINT>(position);

        util::Logger::g_logger.assert_with_log(
            index < DESCRIPTOR_COUNT,
            "invalid sampler descriptor request");

        device_->CreateSampler(&desc, this->get_cpu_adr(position));
    }
}
