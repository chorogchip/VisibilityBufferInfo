#pragma once

#include <d3d12.h>
#include <wrl.h>

namespace eng {

    class ResourceManagerSampler {
    public:
        enum class EnumDescPos : UINT {
            DONUT_SHADOW,
            DONUT_SHADOW_COMPARISON,
            DONUT_LIGHT_PROBE,
            DONUT_BRDF,
            BENCH_MATERIAL,
            COUNT
        };

        void init(ID3D12Device* device);
        void create_sampler(EnumDescPos position);
        void create_sampler(EnumDescPos position, const D3D12_SAMPLER_DESC& desc);
        void create_sampler_linear_wrap(EnumDescPos position);
        void create_sampler_linear_clamp(EnumDescPos position);
        void create_sampler_linear_border_white(EnumDescPos position);
        void create_sampler_comparison_linear_border_white(EnumDescPos position);

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
