#pragma once

#include <Windows.h>
#include <array>
#include <cstdint>
#include <utility>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

#include "util/Constants.h"

namespace dxutl {

    class GpuFrameTimer {

    public:
        static constexpr UINT TOTAL_SLOT = 0;
        static constexpr UINT SLOT_COUNT = util::Constants::TIMER_SLOT_COUNT;
        static constexpr UINT BUF_COUNT = util::Constants::FRAME_COUNT * SLOT_COUNT * 2;

    public:
        void init(
            ID3D12Device* p_device,
            ID3D12CommandQueue* p_queue);

        std::vector<double> read_timestamp(UINT frame_index);
        void start_timestamp(ID3D12GraphicsCommandList* p_list, UINT frame_index, UINT slot);
        void end_timestamp(ID3D12GraphicsCommandList* p_list, UINT frame_index, UINT slot);

    private:
        Microsoft::WRL::ComPtr<ID3D12QueryHeap> query_heap_;
        Microsoft::WRL::ComPtr<ID3D12Resource> readback_buffer_;
        UINT64* readback_buffer_mapped_ = nullptr;
        double timestamp_frequency_rcp_ = 0;
        std::array<std::array<bool, SLOT_COUNT>, util::Constants::FRAME_COUNT> resolved_{};
    };

}
