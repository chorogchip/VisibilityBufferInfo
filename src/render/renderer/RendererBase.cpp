#include "render/renderer/RendererBase.h"

#include <chrono>
#include <ctime>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

#include "util/Constants.h"
#include "util/Utils.h"
#include "util/TimeUtils.h"
#include "util/BenchmarkCsvWriter.h"
#include "dx_util/DeviceUtils.h"
#include "engine/TextureLoader.h"

#include "util/minmax_remover.h"

RendererBase::~RendererBase() {
    graphics_queue_.wait_idle();
}

void RendererBase::init(HWND hwnd, const util::ProgramArgument& arg) {

    program_argument_ = arg;
    program_result_ = util::ProgramResult::from_args(arg);
    program_result_.pass_names[dxutl::GpuFrameTimer::TOTAL_SLOT] = "total";

    hwnd_ = hwnd;
    width_ = arg.window_width;
    height_ = arg.window_height;

    device_ = dxutl::create_device(factory_);
    graphics_queue_.init(device_.Get());

    for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
        util::Utils::throw_if_failed(device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(command_allocator_[i].ReleaseAndGetAddressOf())),
            "create command allocator");
    }

    util::Utils::throw_if_failed(device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_allocator_[0].Get(),
        nullptr,
        IID_PPV_ARGS(command_list_.ReleaseAndGetAddressOf())),
        "create command list");

    util::Utils::throw_if_failed(command_list_->Close(), "command list close");

    swapchain_ = dxutl::create_swapchain(
        factory_.Get(),
        graphics_queue_.get(),
        hwnd_,
        width_,
        height_,
        util::Constants::FRAME_COUNT);

    frame_index_ = swapchain_->GetCurrentBackBufferIndex();
    fence_values_[frame_index_] = 1;

    for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i) {
        Microsoft::WRL::ComPtr<ID3D12Resource> back_buffer;
        swapchain_->GetBuffer(i, IID_PPV_ARGS(back_buffer.GetAddressOf()));
        render_targets_[i].init(back_buffer.Get(), D3D12_RESOURCE_STATE_PRESENT);
    }

    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = static_cast<float>(width_);
    viewport_.Height = static_cast<float>(height_);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissor_rect_.left = 0;
    scissor_rect_.top = 0;
    scissor_rect_.right = static_cast<LONG>(width_);
    scissor_rect_.bottom = static_cast<LONG>(height_);

    camera_.init(arg);
    camera_path_controller_.init(arg, camera_);
    frame_counter_.init(arg);
    frame_time_.init(device_.Get(), graphics_queue_.get());

    this->init1_();
}

void RendererBase::close() {
    graphics_queue_.wait_idle();
    this->before_close_();

    const std::string& path = program_argument_.output_filepath;
    if (path == "") return;


    if (camera_path_controller_.is_playback()) {
        const auto windows = frame_counter_.summarize_windows(
            program_argument_.profile_window_frames);
        util::write_windowed_benchmark_csv(
            path + "_" + std::to_string(program_argument_.run_id) + "_result.csv",
            program_result_.pass_names,
            windows);
    }
    camera_path_controller_.close();

    auto results = frame_counter_.summarize();

    static_assert(util::Constants::TIMER_SLOT_COUNT == dxutl::GpuFrameTimer::SLOT_COUNT);
    util::Logger::g_logger.assert_with_log(
        results.size() == util::Constants::TIMER_SLOT_COUNT,
        "unexpected benchmark result count");

    for (size_t i = 0; i < util::Constants::TIMER_SLOT_COUNT; ++i)
        program_result_.pass_time_avg_ms[i] = results[i].time_avg_ms;

    const auto& total = results[dxutl::GpuFrameTimer::TOTAL_SLOT];
    program_result_.total_time_min_ms = total.time_min_ms;
    program_result_.total_time_median_ms = total.time_median_ms;
    program_result_.total_time_max_ms = total.time_max_ms;
    program_result_.total_time_avg_ms = total.time_avg_ms;
    program_result_.total_time_p01_ms = total.time_p01_ms;
    program_result_.total_time_p10_ms = total.time_p10_ms;
    program_result_.total_time_p90_ms = total.time_p90_ms;
    program_result_.total_time_p99_ms = total.time_p99_ms;

    const auto val_cnt = program_argument_.object_count;
    const auto val_ovd = program_argument_.overdraw_count;
    const auto val_div = program_argument_.geometry_div;
    const auto val_alu = program_argument_.alu_calc_count;

    const size_t val_div_p1 = static_cast<size_t>(val_div + 1);

    util::Logger::g_logger.assert_with_log_mul_overflow(val_div_p1, val_div_p1,
        std::numeric_limits<uint32_t>::max(), "val_div ^2 overflow");

    const uint32_t val_div_p1_sq = static_cast<uint32_t>(val_div_p1 * val_div_p1);

    util::Logger::g_logger.assert_with_log_mul_overflow(val_cnt, val_div_p1_sq,
        std::numeric_limits<uint32_t>::max(), "cnt * val_div overflow");

    program_result_.variable_geometry_count = val_cnt * val_div_p1_sq;
    program_result_.variable_overdraw_count = val_ovd;
    program_result_.variable_waste_quad_count = val_div;
    program_result_.variable_alu_op_count = val_alu;

    util::write_benchmark_csv(path, program_argument_, program_result_);
}

void RendererBase::render() {

    camera_path_controller_.before_render();

    this->render_prepare_();

    util::Utils::throw_if_failed(
        command_allocator_[frame_index_]->Reset(),
        "reset command allocator");

    util::Utils::throw_if_failed(
        command_list_->Reset(command_allocator_[frame_index_].Get(), nullptr),
        "command list reset on render start");

    frame_time_.start_timestamp(
        command_list_.Get(), frame_index_, dxutl::GpuFrameTimer::TOTAL_SLOT);

    this->render_record_();

    render_targets_[frame_index_].transition(
        command_list_.Get(),
        D3D12_RESOURCE_STATE_PRESENT);

    frame_time_.end_timestamp(
        command_list_.Get(), frame_index_, dxutl::GpuFrameTimer::TOTAL_SLOT);

    util::Utils::throw_if_failed(command_list_->Close(),
        "command list close on frame end");

    graphics_queue_.execute(command_list_.Get());

    const UINT sync_interval = program_argument_.vsync ? 1 : 0;
    HRESULT hr{};
    if (!program_argument_.vsync) {
        hr = swapchain_->Present(1, 0);
    } else {
        hr = swapchain_->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }
    if (FAILED(hr)) {
        const HRESULT reason = device_->GetDeviceRemovedReason();

        char buffer[256];
        sprintf_s(
            buffer,
            "Present failed: 0x%08X, DeviceRemovedReason: 0x%08X\n",
            static_cast<unsigned>(hr),
            static_cast<unsigned>(reason));
        buffer[255] = 0;
        util::Logger::g_logger.assert_with_log(false, buffer);
    }

    fence_values_[frame_index_] = graphics_queue_.signal();
    frame_index_ = swapchain_->GetCurrentBackBufferIndex();
    graphics_queue_.wait(fence_values_[frame_index_]);

    std::vector<double> measures = frame_time_.read_timestamp(frame_index_);
    frame_counter_.tick(
        measures,
        to_profile_index_count_,
        profile_index_count_);

    camera_path_controller_.after_render();
}
