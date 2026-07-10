#include "dx_util/DX12QueryBuf.h"

#include <cstring>

#include "util/Utils.h"
#include "dx_util/ResourceUtils.h"

namespace dxutl {

    void DX12QueryBuf::record_timestamp(
        ID3D12GraphicsCommandList* list,
        UINT query_index) {
        
        list->EndQuery(query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            query_index);
    }

    void DX12QueryBuf::resolve_timestamp(
        ID3D12GraphicsCommandList* list,
        UINT query_offset, UINT query_count) {

        list->ResolveQueryData(query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            query_offset, query_count,
            readback_buffer_.Get(), sizeof(UINT64) * query_offset);
    }

    void DX12QueryBuf::readback_timestamp(
        UINT64* dest,
        UINT query_offset, UINT query_count) {

        std::memcpy(
            dest,
            readback_buffer_mapped_ + query_offset,
            query_count * sizeof(UINT64));
    }

    DX12QueryBuf DX12QueryBuf::create_query_buf(
        ID3D12Device* device,
        ID3D12CommandQueue* queue,
        UINT query_count) {

        DX12QueryBuf ret{};

        ret.query_count_ = query_count;

        D3D12_QUERY_HEAP_DESC query_heap_desc{};
        query_heap_desc.Count = query_count;
        query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        query_heap_desc.NodeMask = 0;


        Utils::throw_if_failed(
            device->CreateQueryHeap(&query_heap_desc,
                IID_PPV_ARGS(ret.query_heap_.ReleaseAndGetAddressOf())),
            "create timestamp query heap");

        ret.readback_buffer_ = dxutl::create_buffer(
            device,
            sizeof(UINT64) * query_count,
            D3D12_HEAP_TYPE_READBACK,
            D3D12_RESOURCE_STATE_COPY_DEST);

        UINT64 frequency;
        Utils::throw_if_failed(queue->GetTimestampFrequency(&frequency),
            "get timestamp frequency");

        ret.frequency_rcp_ = 1.0 / static_cast<double>(frequency);

        D3D12_RANGE read_range{ 0, sizeof(UINT64) * query_count };
        Utils::throw_if_failed(ret.readback_buffer_->Map(
            0,
            &read_range,
            reinterpret_cast<void**>(&ret.readback_buffer_mapped_)),
            "map timestamp readback");

        return ret;
    }
}
