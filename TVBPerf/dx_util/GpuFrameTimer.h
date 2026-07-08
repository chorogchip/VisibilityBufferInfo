#pragma once

#include <Windows.h>
#include <cstdint>
#include <utility>
#include <wrl.h>
#include <d3d12.h>

namespace dxutl {

    class GpuFrameTimer {

    public:
        static constexpr UINT FRAME_COUNT = 2;
        static constexpr UINT PASS_COUNT = 2;
        static constexpr UINT BUF_COUNT = FRAME_COUNT * PASS_COUNT * 2;

    public:
        void init(ID3D12Device* p_device, ID3D12CommandQueue* p_queue);
        std::pair<double, double> read_timestamp(UINT frame_index);
        void start_timestamp(ID3D12GraphicsCommandList* p_list, UINT frame_index, UINT pass);
        void end_timestamp(ID3D12GraphicsCommandList* p_list, UINT frame_index, UINT pass);

    private:
        Microsoft::WRL::ComPtr<ID3D12QueryHeap> query_heap_;
        Microsoft::WRL::ComPtr<ID3D12Resource> readback_buffer_;
        UINT64* readback_buffer_mapped_ = nullptr;
        double timestamp_frequency_rcp_ = 0;
    };

}