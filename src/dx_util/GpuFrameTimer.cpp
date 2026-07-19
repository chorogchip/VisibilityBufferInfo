#include "dx_util/GpuFrameTimer.h"

#include "util/Utils.h"
#include "util/Logger.h"
#include "dx_util/ResourceUtils.h"

namespace dxutl {

	void GpuFrameTimer::init(ID3D12Device* p_device, ID3D12CommandQueue* p_queue) {

        D3D12_QUERY_HEAP_DESC query_heap_desc{};
        query_heap_desc.Count = BUF_COUNT;
        query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        query_heap_desc.NodeMask = 0;

        Utils::throw_if_failed(
            p_device->CreateQueryHeap(&query_heap_desc, IID_PPV_ARGS(query_heap_.ReleaseAndGetAddressOf())),
            "create timestamp query heap");

        readback_buffer_ = dxutl::create_buffer(p_device, sizeof(UINT64) * BUF_COUNT, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);

        UINT64 frequency;
        Utils::throw_if_failed(p_queue->GetTimestampFrequency(&frequency),
            "get timestamp frequency");

        timestamp_frequency_rcp_ = 1.0 / static_cast<double>(frequency);

        D3D12_RANGE read_range{ 0, sizeof(UINT64) * BUF_COUNT };
        Utils::throw_if_failed(readback_buffer_->Map(0, &read_range, reinterpret_cast<void**>(&readback_buffer_mapped_)),
            "map timestamp readback");
	}

    std::vector<double> GpuFrameTimer::read_timestamp(UINT frame_index) {

        std::vector<double> ret(PASS_COUNT, 0.0);
        const UINT timestamp_base = frame_index * PASS_COUNT * 2;

        for (UINT pass = 0; pass < PASS_COUNT; ++pass) {
            if (!resolved_[frame_index][pass]) continue;

            const UINT timestamp_index = timestamp_base + pass * 2;
            const UINT64 start = readback_buffer_mapped_[timestamp_index + 0];
            const UINT64 end = readback_buffer_mapped_[timestamp_index + 1];

            if (end > start && timestamp_frequency_rcp_ != 0.0f)
                ret[pass] = static_cast<double>(end - start) * 1000.0 * timestamp_frequency_rcp_;
        }

        return ret;
    }

    void GpuFrameTimer::start_timestamp(ID3D12GraphicsCommandList* p_list, UINT frame_index, UINT pass) {

        util::Logger::g_logger.assert_with_log(
            frame_index < FRAME_COUNT && pass < PASS_COUNT,
            "GPU timestamp index is out of range");

        const UINT timestamp_index = frame_index * PASS_COUNT * 2 + pass * 2;

        p_list->EndQuery(query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, timestamp_index);
    }

    void GpuFrameTimer::end_timestamp(ID3D12GraphicsCommandList* p_list, UINT frame_index, UINT pass) {

        util::Logger::g_logger.assert_with_log(
            frame_index < FRAME_COUNT && pass < PASS_COUNT,
            "GPU timestamp index is out of range");

        const UINT timestamp_index = frame_index * PASS_COUNT * 2 + pass * 2;

        p_list->EndQuery(query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, timestamp_index + 1);
        p_list->ResolveQueryData(query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            timestamp_index, 2,
            readback_buffer_.Get(), sizeof(UINT64) * timestamp_index);
        resolved_[frame_index][pass] = true;
    }
}
