#pragma once

#include <Windows.h>
#include <cstdint>
#include <utility>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

namespace dxutl {

    class DX12QueryBuf {

    private:
        Microsoft::WRL::ComPtr<ID3D12QueryHeap> query_heap_;
        Microsoft::WRL::ComPtr<ID3D12Resource> readback_buffer_;
        UINT64* readback_buffer_mapped_ = nullptr;
        UINT query_count_ = 0;
        double frequency_rcp_ = 0.0;

    public:
        DX12QueryBuf() = default;

        DX12QueryBuf(const DX12QueryBuf&) = delete;
        DX12QueryBuf& operator=(const DX12QueryBuf&) = delete;

        DX12QueryBuf(DX12QueryBuf&&) noexcept = default;
        DX12QueryBuf& operator=(DX12QueryBuf&&) noexcept = default;

        void record_timestamp(
            ID3D12GraphicsCommandList* list,
            UINT query_index);

        void resolve_timestamp(
            ID3D12GraphicsCommandList* list,
            UINT query_offset, UINT query_count);

        void readback_timestamp(
            UINT64* dest,
            UINT query_offset, UINT query_count);
        double ticks_to_seconds(UINT64 ticks) const noexcept {
            return static_cast<double>(ticks) * frequency_rcp_;
        }

        static DX12QueryBuf create_query_buf(
            ID3D12Device* device,
            ID3D12CommandQueue* queue,
            UINT query_count);
    };
}