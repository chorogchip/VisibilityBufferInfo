#pragma once

#include <Windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <wincodec.h>
#include <wrl.h>
#include <d3d12.h>

#include "ProgramArgument.h"
#include "engine/GPUResource.h"
#include "util/Constants.h"

namespace rndr {

    class FrameCapture {
        template <typename T>
        using ComPtr = Microsoft::WRL::ComPtr<T>;

    public:
        FrameCapture() = default;
        FrameCapture(const FrameCapture&) = delete;
        FrameCapture& operator=(const FrameCapture&) = delete;

        void init(
            ID3D12Device* device,
            const util::ProgramArgument& argument,
            ID3D12Resource* sample_back_buffer);
        void process_completed(UINT frame_index);
        void capture(
            ID3D12GraphicsCommandList* command_list,
            UINT frame_index,
            eng::GPUResource& back_buffer);
        void close();

        [[nodiscard]] bool enabled() const { return enabled_; }

    private:
        struct PendingFrame {
            bool pending = false;
            std::uint64_t render_frame = 0;
            std::uint64_t measurement_frame = 0;
            std::uint64_t image_index = 0;
        };

        struct CapturedFrame {
            std::uint64_t image_index = 0;
            std::uint64_t render_frame = 0;
            std::uint64_t measurement_frame = 0;
            std::filesystem::path path{};
        };

        void write_pending_(UINT frame_index);
        void save_png_(
            const std::filesystem::path& path,
            const std::byte* pixels,
            UINT row_pitch) const;
        void write_manifest_() const;
        void write_encode_script_() const;

        bool enabled_ = false;
        std::uint32_t width_ = 0;
        std::uint32_t height_ = 0;
        std::uint32_t warmup_frames_ = 0;
        std::uint32_t measure_frames_ = 0;
        std::uint32_t capture_stride_ = 1;
        std::uint32_t capture_fps_ = 60;
        std::uint64_t render_frame_ = 0;
        std::uint64_t image_index_ = 0;
        DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
        UINT row_count_ = 0;
        UINT64 row_size_ = 0;
        UINT64 readback_size_ = 0;

        std::filesystem::path output_dir_{};
        std::filesystem::path frames_dir_{};
        std::array<ComPtr<ID3D12Resource>, util::Constants::FRAME_COUNT>
            readback_buffers_{};
        std::array<PendingFrame, util::Constants::FRAME_COUNT> pending_{};
        std::vector<CapturedFrame> captured_frames_{};
        ComPtr<IWICImagingFactory> wic_factory_{};
    };

}
