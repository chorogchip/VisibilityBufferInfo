#pragma once

#include <cstring>
#include <type_traits>

#include <d3d12.h>
#include <wrl.h>

#include "util/Assume.h"
#include "util/Logger.h"
#include "dx_util/ResourceUtils.h"

namespace dxutl {

    template<typename T>
    class UploadConstBuf {
        static_assert(std::is_trivially_copyable_v<T>,
            "constant-buffer data must be trivially copyable");

    public:
        UploadConstBuf() = default;
        UploadConstBuf(const UploadConstBuf&) = delete;
        UploadConstBuf& operator=(const UploadConstBuf&) = delete;
        UploadConstBuf(UploadConstBuf&&) = delete;
        UploadConstBuf& operator=(UploadConstBuf&&) = delete;

        ~UploadConstBuf() {
            resource_->Unmap(0, nullptr);
        }

        void init(ID3D12Device* device) {
            util::Logger::g_logger.assert_with_log(
                device != nullptr, "upload constant buffer requires a device");
            util::Logger::g_logger.assert_with_log(
                !resource_, "upload constant buffer is already initialized");

            resource_ = create_buffer(
                device,
                ALIGNED_SIZE,
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_STATE_GENERIC_READ);

            mapped_ = map_upload_buffer(resource_.Get());
        }

        void update() {
            util::Logger::g_logger.assert_with_log(
                mapped_ != nullptr,
                "upload constant buffer is not initialized");

            MSVC_ASSUME(mapped_ != nullptr);
            std::memcpy(mapped_, &this->buffer, sizeof(T));
        }

        [[nodiscard]] ID3D12Resource* get() const { return resource_.Get(); }

        T buffer;
    private:
        static constexpr UINT64 ALIGNMENT = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        static constexpr UINT64 ALIGNED_SIZE =
            (static_cast<UINT64>(sizeof(T)) + ALIGNMENT - 1) & ~(ALIGNMENT - 1);

        Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
        void* mapped_ = nullptr;
    };

}
