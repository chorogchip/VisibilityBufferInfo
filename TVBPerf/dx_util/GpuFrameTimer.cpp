#include "dx_util/GpuFrameTimer.h"

#include "util/Utils.h"
#include "dx_util/GraphicsUtils.h"

namespace dxutl {

	void GpuFrameTimer::init(ID3D12Device* p_device, ID3D12CommandQueue* p_queue) {

        D3D12_QUERY_HEAP_DESC query_heap_desc{};
        query_heap_desc.Count = BUF_COUNT;
        query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        query_heap_desc.NodeMask = 0;

        Utils::throw_if_failed(
            p_device->CreateQueryHeap(&query_heap_desc, IID_PPV_ARGS(query_heap_.ReleaseAndGetAddressOf())),
            "create timestamp query heap");

        GraphicsUtils::create_buffer(
            readback_buffer_, p_device, sizeof(UINT64) * BUF_COUNT, 1,
            D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);

        UINT64 frequency;
        Utils::throw_if_failed(p_queue->GetTimestampFrequency(&frequency),
            "get timestamp frequency");

        timestamp_frequency_rcp_ = 1.0 / static_cast<double>(frequency);

        D3D12_RANGE read_range{ 0, 0 };
        Utils::throw_if_failed(readback_buffer_->Map(0, &read_range, reinterpret_cast<void**>(&readback_buffer_mapped_)),
            "map timestamp readback");
	}

    std::pair<double,double> GpuFrameTimer::read_timestamp(UINT frame_index) {

        std::pair<double, double> ret{};
        const UINT timestamp_base = frame_index * PASS_COUNT * 2;

        const UINT64 start0 = readback_buffer_mapped_[timestamp_base + 0];
        const UINT64 end0 = readback_buffer_mapped_[timestamp_base + 1];
        const UINT64 start1 = readback_buffer_mapped_[timestamp_base + 2];
        const UINT64 end1 = readback_buffer_mapped_[timestamp_base + 3];

        if (end0 > start0 && timestamp_frequency_rcp_ != 0.0f)
            ret.first = static_cast<double>(end0 - start0) * 1000.0 * timestamp_frequency_rcp_;

        if (end1 > start1 && timestamp_frequency_rcp_ != 0.0f)
            ret.second = static_cast<double>(end1 - start1) * 1000.0 * timestamp_frequency_rcp_;

        return ret;
    }

    void GpuFrameTimer::start_timestamp(ID3D12GraphicsCommandList* p_list, UINT frame_index, UINT pass) {

        const UINT timestamp_index = frame_index * PASS_COUNT * 2 + pass * 2;

        p_list->EndQuery(query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, timestamp_index);
    }

    void GpuFrameTimer::end_timestamp(ID3D12GraphicsCommandList* p_list, UINT frame_index, UINT pass) {

        const UINT timestamp_index = frame_index * PASS_COUNT * 2 + pass * 2;

        p_list->EndQuery(query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, timestamp_index + 1);
        p_list->ResolveQueryData(query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            timestamp_index, 2,
            readback_buffer_.Get(), sizeof(UINT64) * timestamp_index);
    }
}