#pragma once

#include <array>
#include <d3d12.h>
#include <wrl.h>

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
        void request_rtv(EnumRTV position, ID3D12Resource* texture);
        void request_dsv(EnumDSV position, ID3D12Resource* texture);
        void build();

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get_rtv(EnumRTV position) const;
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get_dsv(EnumDSV position) const;

    private:
        static constexpr UINT RTV_COUNT = static_cast<UINT>(EnumRTV::COUNT);
        static constexpr UINT DSV_COUNT = static_cast<UINT>(EnumDSV::COUNT);
        static constexpr UINT INVALID_INDEX = UINT_MAX;

        ID3D12Device* device_ = nullptr;
        std::array<UINT, RTV_COUNT> rtv_indices_{};
        std::array<UINT, DSV_COUNT> dsv_indices_{};
        std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, RTV_COUNT> rtv_resources_;
        std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, DSV_COUNT> dsv_resources_;
        UINT rtv_size_ = 0;
        UINT dsv_size_ = 0;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap_;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsv_heap_;
    };

}
