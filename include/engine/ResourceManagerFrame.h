#pragma once

#include <d3d12.h>
#include <wrl.h>

#include "Constants.h"

namespace eng {

    class ResourceManagerFrame {
    public:
        enum class EnumRTV : UINT {
            BACK_BUFFER_0,
            BACK_BUFFER_1,
            DONUT_GBUFFER_0,
            DONUT_GBUFFER_1,
            DONUT_GBUFFER_2,
            DONUT_GBUFFER_3,
            DONUT_HDR_COLOR,
            DONUT_MOTION_VECTOR,

            BENCH_VISIBILITY,
            BENCH_GBUFFER_0,
            BENCH_GBUFFER_1,
            BENCH_GBUFFER_2,
            BENCH_GBUFFER_3,
            BENCH_GBUFFER_4,
            BENCH_GBUFFER_5,
            BENCH_GBUFFER_6,
            BENCH_GBUFFER_7,
            COUNT
        };

        enum class EnumDSV : UINT {
            DEPTH,
            DEPTH_READ_ONLY,
            COUNT
        };

        void init(ID3D12Device* device);
        void create_rtv(EnumRTV position, ID3D12Resource* texture);
        void create_dsv(EnumDSV position, ID3D12Resource* texture);

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get_rtv(EnumRTV position, UINT offset = 0) const;
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get_dsv(EnumDSV position, UINT offset = 0) const;

    private:
        static constexpr UINT RTV_COUNT = static_cast<UINT>(EnumRTV::COUNT);
        static constexpr UINT DSV_COUNT = static_cast<UINT>(EnumDSV::COUNT);
        ID3D12Device* device_ = nullptr;
        UINT rtv_size_ = 0;
        UINT dsv_size_ = 0;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap_;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsv_heap_;
    };

}
