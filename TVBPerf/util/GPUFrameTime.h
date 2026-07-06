#pragma once

#include <Windows.h>
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

template<int FRAME_COUNT>
struct GpuFrameTime {
    static constexpr UINT TIMESTAMP_COUNT_PER_FRAME = 2;
    static constexpr UINT TIMESTAMP_START = 0;
    static constexpr UINT TIMESTAMP_END = 1;
    
    ComPtr<ID3D12QueryHeap> timestamp_query_heap_;
    ComPtr<ID3D12Resource> timestamp_readback_buffer_;

    bool timestamp_frame_valid_[FRAME_COUNT]{};

    UINT64 timestamp_frequency_ = 0;

    struct GpuFrameTimeInfo
    {
        UINT64 start = 0;
        UINT64 end = 0;
        bool valid = false;
    };

    GpuFrameTimeInfo gpu_times_[FRAME_COUNT]{};
    double gpu_frame_ms_[FRAME_COUNT]{};
};