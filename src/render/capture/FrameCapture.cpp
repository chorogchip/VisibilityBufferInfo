#include "render/capture/FrameCapture.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

#include "dx_util/ResourceUtils.h"
#include "util/Logger.h"
#include "util/Utils.h"

namespace rndr {

    namespace {
        std::string json_escape(const std::string& value) {
            std::ostringstream output;
            for (const char c : value) {
                switch (c) {
                case '\\': output << "\\\\"; break;
                case '"': output << "\\\""; break;
                case '\n': output << "\\n"; break;
                case '\r': output << "\\r"; break;
                case '\t': output << "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        output << "\\u"
                            << std::hex << std::setw(4)
                            << std::setfill('0')
                            << static_cast<unsigned>(
                                static_cast<unsigned char>(c))
                            << std::dec << std::setfill(' ');
                    }
                    else {
                        output << c;
                    }
                    break;
                }
            }
            return output.str();
        }

        std::string make_frame_filename(std::uint64_t image_index) {
            std::ostringstream output;
            output << "frame_" << std::setw(6) << std::setfill('0')
                << image_index << ".png";
            return output.str();
        }
    }

    void FrameCapture::init(
        ID3D12Device* device,
        const util::ProgramArgument& argument,
        ID3D12Resource* sample_back_buffer) {

        enabled_ = argument.capture_frames;
        if (!enabled_) return;

        util::Logger::g_logger.assert_with_log(
            device != nullptr && sample_back_buffer != nullptr,
            "frame capture requires a device and back buffer");

        warmup_frames_ = argument.warmup_frames;
        measure_frames_ = argument.measure_frames;
        capture_stride_ = argument.capture_stride;
        capture_fps_ = argument.capture_fps;

        const D3D12_RESOURCE_DESC desc = sample_back_buffer->GetDesc();
        util::Logger::g_logger.assert_with_log(
            desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
            desc.DepthOrArraySize == 1 &&
            desc.MipLevels == 1 &&
            desc.SampleDesc.Count == 1,
            "frame capture supports single-sample 2D back buffers only");
        util::Logger::g_logger.assert_with_log(
            desc.Width <= (std::numeric_limits<std::uint32_t>::max)() &&
            desc.Height <= (std::numeric_limits<std::uint32_t>::max)(),
            "frame capture back buffer dimensions exceed 32-bit size");
        util::Logger::g_logger.assert_with_log(
            desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM,
            "frame capture currently supports DXGI_FORMAT_R8G8B8A8_UNORM only");

        width_ = static_cast<std::uint32_t>(desc.Width);
        height_ = static_cast<std::uint32_t>(desc.Height);
        format_ = desc.Format;

        device->GetCopyableFootprints(
            &desc,
            0,
            1,
            0,
            &footprint_,
            &row_count_,
            &row_size_,
            &readback_size_);
        util::Logger::g_logger.assert_with_log(
            row_count_ == height_ &&
            row_size_ <= footprint_.Footprint.RowPitch,
            "unexpected frame capture readback footprint");

        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
            readback_buffers_[i] = dxutl::create_buffer(
                device,
                readback_size_,
                D3D12_HEAP_TYPE_READBACK,
                D3D12_RESOURCE_STATE_COPY_DEST);
        }

        output_dir_ = argument.capture_output_dir.empty()
            ? std::filesystem::path("captures")
            : std::filesystem::path(argument.capture_output_dir);
        frames_dir_ = output_dir_ / "frames";
        std::filesystem::create_directories(frames_dir_);

        util::Utils::throw_if_failed(
            CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(wic_factory_.ReleaseAndGetAddressOf())),
            "create WIC imaging factory for frame capture");

        util::Logger::g_logger
            << "Frame capture enabled: "
            << frames_dir_.string()
            << " stride{" << capture_stride_
            << "} fps{" << capture_fps_
            << "}\n";
    }

    void FrameCapture::process_completed(UINT frame_index) {
        if (!enabled_) return;
        write_pending_(frame_index);
    }

    void FrameCapture::capture(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        eng::GPUResource& back_buffer) {

        if (!enabled_) return;

        util::Logger::g_logger.assert_with_log(
            command_list != nullptr &&
            frame_index < util::Constants::FRAME_COUNT &&
            back_buffer.get() != nullptr,
            "frame capture requires a valid command list, frame index, and back buffer");
        util::Logger::g_logger.assert_with_log(
            !pending_[frame_index].pending,
            "frame capture readback slot was reused before being written");

        const std::uint64_t current_render_frame = render_frame_++;
        if (current_render_frame < warmup_frames_)
            return;

        const std::uint64_t measurement_frame =
            current_render_frame - warmup_frames_;
        if (measurement_frame >= measure_frames_)
            return;
        if (measurement_frame % capture_stride_ != 0)
            return;

        back_buffer.transition(command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);

        D3D12_TEXTURE_COPY_LOCATION target{};
        target.pResource = readback_buffers_[frame_index].Get();
        target.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        target.PlacedFootprint = footprint_;

        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = back_buffer.get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        source.SubresourceIndex = 0;

        command_list->CopyTextureRegion(
            &target,
            0,
            0,
            0,
            &source,
            nullptr);

        const std::uint64_t captured_image_index = image_index_++;
        const std::filesystem::path path =
            frames_dir_ / make_frame_filename(captured_image_index);
        pending_[frame_index] = {
            true,
            current_render_frame,
            measurement_frame,
            captured_image_index
        };
        captured_frames_.push_back({
            captured_image_index,
            current_render_frame,
            measurement_frame,
            path
        });
    }

    void FrameCapture::close() {
        if (!enabled_) return;

        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i)
            write_pending_(i);

        write_manifest_();
        write_encode_script_();
        util::Logger::g_logger
            << "Frame capture saved " << captured_frames_.size()
            << " frame(s) to " << frames_dir_.string() << '\n';
    }

    void FrameCapture::write_pending_(UINT frame_index) {
        if (!pending_[frame_index].pending)
            return;

        const PendingFrame pending = pending_[frame_index];
        const std::filesystem::path path =
            frames_dir_ / make_frame_filename(pending.image_index);

        void* mapped = nullptr;
        const D3D12_RANGE read_range{
            0,
            static_cast<SIZE_T>(readback_size_)
        };
        util::Utils::throw_if_failed(
            readback_buffers_[frame_index]->Map(0, &read_range, &mapped),
            "map frame capture readback buffer");

        const std::byte* pixels =
            static_cast<const std::byte*>(mapped) + footprint_.Offset;
        save_png_(path, pixels, footprint_.Footprint.RowPitch);

        const D3D12_RANGE written_range{ 0, 0 };
        readback_buffers_[frame_index]->Unmap(0, &written_range);
        pending_[frame_index].pending = false;
    }

    void FrameCapture::save_png_(
        const std::filesystem::path& path,
        const std::byte* pixels,
        UINT row_pitch) const {

        util::Logger::g_logger.assert_with_log(
            wic_factory_ != nullptr && pixels != nullptr,
            "frame capture PNG save requires WIC and pixel data");
        util::Logger::g_logger.assert_with_log(
            static_cast<std::uint64_t>(row_pitch) * height_ <=
            (std::numeric_limits<UINT>::max)(),
            "frame capture PNG buffer is too large for WIC");

        std::filesystem::create_directories(path.parent_path());

        ComPtr<IWICStream> stream;
        util::Utils::throw_if_failed(
            wic_factory_->CreateStream(stream.ReleaseAndGetAddressOf()),
            "create WIC frame capture stream");
        util::Utils::throw_if_failed(
            stream->InitializeFromFilename(path.wstring().c_str(), GENERIC_WRITE),
            "initialize WIC frame capture stream");

        ComPtr<IWICBitmapEncoder> encoder;
        util::Utils::throw_if_failed(
            wic_factory_->CreateEncoder(
                GUID_ContainerFormatPng,
                nullptr,
                encoder.ReleaseAndGetAddressOf()),
            "create WIC PNG encoder");
        util::Utils::throw_if_failed(
            encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache),
            "initialize WIC PNG encoder");

        ComPtr<IWICBitmapFrameEncode> frame;
        util::Utils::throw_if_failed(
            encoder->CreateNewFrame(frame.ReleaseAndGetAddressOf(), nullptr),
            "create WIC PNG frame");
        util::Utils::throw_if_failed(
            frame->Initialize(nullptr),
            "initialize WIC PNG frame");
        util::Utils::throw_if_failed(
            frame->SetSize(width_, height_),
            "set WIC PNG frame size");

        WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppRGBA;
        util::Utils::throw_if_failed(
            frame->SetPixelFormat(&pixel_format),
            "set WIC PNG pixel format");
        util::Logger::g_logger.assert_with_log(
            IsEqualGUID(pixel_format, GUID_WICPixelFormat32bppRGBA),
            "WIC PNG encoder changed the requested frame capture pixel format");

        const UINT buffer_size = static_cast<UINT>(
            static_cast<std::uint64_t>(row_pitch) * height_);
        util::Utils::throw_if_failed(
            frame->WritePixels(
                height_,
                row_pitch,
                buffer_size,
                reinterpret_cast<BYTE*>(const_cast<std::byte*>(pixels))),
            "write WIC PNG pixels");
        util::Utils::throw_if_failed(frame->Commit(), "commit WIC PNG frame");
        util::Utils::throw_if_failed(encoder->Commit(), "commit WIC PNG encoder");
    }

    void FrameCapture::write_manifest_() const {
        const std::filesystem::path path = output_dir_ / "capture_manifest.json";
        std::ofstream output(path, std::ios::out | std::ios::trunc);
        util::Logger::g_logger.assert_with_log(
            static_cast<bool>(output),
            "failed to open frame capture manifest");

        output << "{\n";
        output << "  \"format\": \"png\",\n";
        output << "  \"pixel_format\": \"DXGI_FORMAT_R8G8B8A8_UNORM\",\n";
        output << "  \"width\": " << width_ << ",\n";
        output << "  \"height\": " << height_ << ",\n";
        output << "  \"warmup_frames\": " << warmup_frames_ << ",\n";
        output << "  \"measure_frames\": " << measure_frames_ << ",\n";
        output << "  \"capture_stride\": " << capture_stride_ << ",\n";
        output << "  \"capture_fps\": " << capture_fps_ << ",\n";
        output << "  \"captured_frame_count\": "
            << captured_frames_.size() << ",\n";
        output << "  \"frame_name_pattern\": \"frames/frame_%06d.png\",\n";
        output << "  \"frame_mapping\": "
            << "\"measurement_frame = image_index * capture_stride\",\n";
        output << "  \"frames\": [\n";
        for (size_t i = 0; i < captured_frames_.size(); ++i) {
            const CapturedFrame& frame = captured_frames_[i];
            output << "    {"
                << "\"image_index\": " << frame.image_index
                << ", \"render_frame\": " << frame.render_frame
                << ", \"measurement_frame\": " << frame.measurement_frame
                << ", \"path\": \""
                << json_escape(frame.path.generic_string())
                << "\"}";
            if (i + 1 < captured_frames_.size())
                output << ',';
            output << '\n';
        }
        output << "  ]\n";
        output << "}\n";

        util::Logger::g_logger.assert_with_log(
            static_cast<bool>(output),
            "failed to write frame capture manifest");
    }

    void FrameCapture::write_encode_script_() const {
        const std::filesystem::path path = output_dir_ / "encode_capture.ps1";
        std::ofstream output(path, std::ios::out | std::ios::trunc);
        util::Logger::g_logger.assert_with_log(
            static_cast<bool>(output),
            "failed to open frame capture encode script");

        output << "$ErrorActionPreference = 'Stop'\n";
        output << "$root = Split-Path -Parent $MyInvocation.MyCommand.Path\n";
        output << "$frames = Join-Path $root 'frames/frame_%06d.png'\n";
        output << "$video = Join-Path $root 'video.mp4'\n";
        output << "ffmpeg -y -framerate " << capture_fps_
            << " -i $frames -frames:v " << captured_frames_.size()
            << " -c:v libx264 -pix_fmt yuv420p -crf 18 $video\n";

        util::Logger::g_logger.assert_with_log(
            static_cast<bool>(output),
            "failed to write frame capture encode script");
    }

}
