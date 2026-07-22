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
            DONUT_MATERIAL,
            BENCH_MATERIAL,
            COUNT
        };

        enum class EnumSamplerType {
            LINEAR_WRAP,
            LINEAR_CLAMP,
            LINEAR_BORDER_WHITE,
            LINEAR_BORDER_WHITE_COMP,
        };

        void init(ID3D12Device* device);
        void create_sampler(EnumDescPos position, EnumSamplerType type);
        void create_sampler(EnumDescPos position, const D3D12_SAMPLER_DESC& desc);

        [[nodiscard]] ID3D12DescriptorHeap* get() const { return heap_.Get(); }

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_adr(EnumDescPos position) const {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = heap_->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += static_cast<SIZE_T>(static_cast<UINT>(position)) * descriptor_size_;
            return handle;
        }

        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_adr(EnumDescPos position) const {
            D3D12_GPU_DESCRIPTOR_HANDLE handle = heap_->GetGPUDescriptorHandleForHeapStart();
            handle.ptr += static_cast<UINT64>(static_cast<UINT>(position)) * descriptor_size_;
            return handle;
        }

    private:
        static constexpr UINT DESCRIPTOR_COUNT = static_cast<UINT>(EnumDescPos::COUNT);

        ID3D12Device* device_ = nullptr;
        UINT descriptor_size_ = 0;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    };

}
