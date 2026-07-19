#pragma once

#include <d3d12.h>
#include <wrl.h>

namespace eng {

    class ResourceManagerSampler {
    public:
        enum class EnumDescPos : UINT {
            BENCH_MATERIAL = 0,
            DONUT_SHADOW = 1,
            DONUT_SHADOW_COMPARISON = 2,
            DONUT_LIGHT_PROBE = 3,
            DONUT_BRDF = 4,
            COUNT
        };

        void init(ID3D12Device* device);
        void request(EnumDescPos position, const D3D12_SAMPLER_DESC& desc);

        [[nodiscard]] ID3D12DescriptorHeap* get() const { return heap_.Get(); }
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_adr(EnumDescPos position) const;
        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_adr(EnumDescPos position) const;

    private:
        static constexpr UINT DESCRIPTOR_COUNT = static_cast<UINT>(EnumDescPos::COUNT);

        ID3D12Device* device_ = nullptr;
        UINT descriptor_size_ = 0;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    };

}
