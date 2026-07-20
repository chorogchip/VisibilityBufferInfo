#include "render/renderer/RendererBase.h"

#include <chrono>
#include <ctime>
#include <algorithm>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>

#include "util/Utils.h"
#include "util/BenchmarkCsvWriter.h"
#include "dx_util/DeviceUtils.h"
#include "dx_util/ResourceUtils.h"
#include "engine/TextureLoader.h"

#include "util/Macros.h"

using Microsoft::WRL::ComPtr;

namespace {
    std::string make_current_time_string() {
        const auto now = std::chrono::system_clock::now();
        const std::time_t current_time = std::chrono::system_clock::to_time_t(now);
        std::tm local_time{};
        localtime_s(&local_time, &current_time);

        std::ostringstream stream;
        stream << std::put_time(&local_time, "%Y-%m-%dT%H:%M:%S");
        return stream.str();
    }
}

RendererBase::~RendererBase() {
    graphics_queue_.wait_idle();
    eng::TextureLoader::close();
}

void RendererBase::init(HWND hwnd, const util::ProgramArgument& arg) {
    init_runtime(hwnd, arg);
    init_gpu(arg);
    init_scene_();
    init_shader_resources_();
    init_renderer();
}

void RendererBase::init_runtime(HWND hwnd, const util::ProgramArgument& arg) {
    eng::TextureLoader::init();

    hwnd_ = hwnd;
    width_ = arg.window_width;
    height_ = arg.window_height;

    program_arguments_ = std::make_unique<const util::ProgramArgument>(arg);
    camera_path_controller_.init(arg);

    frame_counter_.init(
        dxutl::GpuFrameTimer::PASS_COUNT + 1,
        arg.warmup_frames,
        arg.warmup_frames + camera_path_controller_.measurement_frames(),
        arg.warmup_frames + camera_path_controller_.measurement_frames() + 60);
}

void RendererBase::init_gpu(const util::ProgramArgument& arg) {
    (void)arg;
    device_ = dxutl::create_device(factory_);
    resource_manager_frame_.init(device_.Get());
    resource_manager_sampler_.init(device_.Get());

    this->create_command_objects();

    swapchain_ = dxutl::create_swapchain(
        factory_.Get(),
        graphics_queue_.get(),
        hwnd_,
        width_,
        height_,
        util::FRAME_COUNT);

    frame_index_ = swapchain_->GetCurrentBackBufferIndex();

    fence_values_[frame_index_] = 1;
    frame_time_.init(device_.Get(), graphics_queue_.get());

    this->init_viewport_scissor_rect();
}

void RendererBase::init_renderer() {
    this->init_renderer_resources_();
    this->create_frame_resources();
    this->create_back_buffer_resources();
    this->init_passes_();
}

void RendererBase::render() {

    camera_path_controller_.before_render(camera_);
    copy_camera_data();
    begin_frame();
    this->record_render_commands_();
    this->transition_back_buffers_();
    submit_frame();
    present();
    this->move_to_next_frame();
    camera_path_controller_.after_render();
}

void RendererBase::close() {
    graphics_queue_.wait_idle();
    camera_path_controller_.close(camera_);

    const std::string& path = program_arguments_->output_filepath;
    if (path == "") return;

    util::ProgramResult result{};
    result.run_current_time = make_current_time_string();
    result.camera_mode_name = util::ProgramArgument::camera_mode_to_string(
        program_arguments_->camera_mode);
    this->init_programresult_(result);

    if (camera_path_controller_.is_playback()) {
        const auto windows = frame_counter_.summarize_windows(
            program_arguments_->profile_window_frames);
        util::write_windowed_benchmark_csv(path, result.pass_names, windows);
        return;
    }

    auto results = frame_counter_.summarize();

    static_assert(util::ProgramResult::PASS_COUNT == dxutl::GpuFrameTimer::PASS_COUNT);
    util::Logger::g_logger.assert_with_log(
        results.size() == util::ProgramResult::PASS_COUNT + 1,
        "unexpected benchmark result count");

    for (size_t i = 0; i < util::ProgramResult::PASS_COUNT; ++i)
        result.pass_time_avg_ms[i] = results[i].time_avg_ms;

    const auto& total = results[util::ProgramResult::PASS_COUNT];
    result.total_time_min_ms = total.time_min_ms;
    result.total_time_median_ms = total.time_median_ms;
    result.total_time_max_ms = total.time_max_ms;
    result.total_time_avg_ms = total.time_avg_ms;
    result.total_time_p01_ms = total.time_p01_ms;
    result.total_time_p10_ms = total.time_p10_ms;
    result.total_time_p90_ms = total.time_p90_ms;
    result.total_time_p99_ms = total.time_p99_ms;

    const auto val_cnt = program_arguments_->object_count;
    const auto val_ovd = program_arguments_->overdraw_count;
    const auto val_div = program_arguments_->geometry_div;
    const auto val_alu = program_arguments_->alu_calc_count;

    const size_t val_div_p1 = static_cast<size_t>(val_div + 1);

    util::Logger::g_logger.assert_with_log_mul_overflow(val_div_p1, val_div_p1,
        std::numeric_limits<uint32_t>::max(), "val_div ^2 overflow");

    const uint32_t val_div_p1_sq = static_cast<uint32_t>(val_div_p1 * val_div_p1);

    util::Logger::g_logger.assert_with_log_mul_overflow(val_cnt, val_div_p1_sq,
        std::numeric_limits<uint32_t>::max(), "cnt * val_div overflow");

    result.variable_geometry_count = val_cnt * val_div_p1_sq;
    result.variable_overdraw_count = val_ovd;
    result.variable_waste_quad_count = val_div;
    result.variable_alu_op_count = val_alu;

    util::write_benchmark_csv(path, *program_arguments_, result);
}

void RendererBase::create_command_objects() {
    graphics_queue_.init(device_.Get());

    for (UINT i = 0; i < util::FRAME_COUNT; ++i) {
        Utils::throw_if_failed(device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(command_allocator_[i].ReleaseAndGetAddressOf())),
            "create command allocator");
    }

    Utils::throw_if_failed(device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_allocator_[0].Get(),
        nullptr,
        IID_PPV_ARGS(command_list_.ReleaseAndGetAddressOf())),
        "create command list");

    Utils::throw_if_failed(command_list_->Close(), "command list close");
}

void RendererBase::begin_frame() {
    Utils::throw_if_failed(
        command_allocator_[frame_index_]->Reset(),
        "reset command allocator");
    Utils::throw_if_failed(
        command_list_->Reset(command_allocator_[frame_index_].Get(), nullptr),
        "command list reset on render start");
}

void RendererBase::submit_frame() {
    Utils::throw_if_failed(
        command_list_->Close(),
        "command list close on frame end");
    graphics_queue_.execute(command_list_.Get());
}

void RendererBase::init_renderer_resources_() {}

void RendererBase::init_viewport_scissor_rect() {
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
}

void RendererBase::create_back_buffer_resources() {
    for (UINT i = 0; i < util::FRAME_COUNT; ++i) {
        ComPtr<ID3D12Resource> back_buffer;
        swapchain_->GetBuffer(i, IID_PPV_ARGS(back_buffer.GetAddressOf()));
        render_targets_[i].attach(back_buffer.Get(), D3D12_RESOURCE_STATE_PRESENT);
    }
}

void RendererBase::transition_back_buffers_() {
    for (auto& render_target : render_targets_)
        render_target.transition(command_list_.Get(), D3D12_RESOURCE_STATE_PRESENT);
}

void RendererBase::create_frame_resources() {

    camera_.set_pos(
        program_arguments_->camera_pos_x,
        program_arguments_->camera_pos_y,
        program_arguments_->camera_pos_z);

    camera_.lookat(
        program_arguments_->camera_lookat_x,
        program_arguments_->camera_lookat_y,
        program_arguments_->camera_lookat_z);

    camera_.set_fovy_nearz_farz(
        program_arguments_->camera_fov,
        program_arguments_->camera_near_z,
        program_arguments_->camera_far_z);

    DirectX::XMStoreFloat4x4(
        &matrix_buf_cpu_.mat_view_,
        DirectX::XMMatrixTranspose(camera_.get_mat_view()));

    DirectX::XMStoreFloat4x4(
        &matrix_buf_cpu_.mat_proj_,
        DirectX::XMMatrixTranspose(camera_.get_mat_proj(width_, height_)));

    matrix_buf_cpu_.viewport_size_ = DirectX::XMFLOAT2(
        static_cast<float>(width_), static_cast<float>(height_));

    for (UINT i = 0; i < util::FRAME_COUNT; ++i)
        buf_constant_[i].init(device_.Get());

    auto depth_stencil_buffer = dxutl::create_depth_stencil_buffer(
        device_.Get(),
        width_,
        height_,
        DEPTH_STENCIL_FORMAT_,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);
    depth_stencil_buffer_.attach(depth_stencil_buffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void RendererBase::copy_camera_data() {

    DirectX::XMStoreFloat4x4(
        &matrix_buf_cpu_.mat_view_,
        DirectX::XMMatrixTranspose(camera_.get_mat_view()));

    DirectX::XMStoreFloat4x4(
        &matrix_buf_cpu_.mat_proj_,
        DirectX::XMMatrixTranspose(camera_.get_mat_proj(width_, height_)));

    matrix_buf_cpu_.viewport_size_ = DirectX::XMFLOAT2(
        static_cast<float>(width_), static_cast<float>(height_));

    buf_constant_[frame_index_].update(matrix_buf_cpu_);
}

void RendererBase::present() {
    const UINT sync_interval = program_arguments_->vsync ? 1 : 0;
    const UINT flags = program_arguments_->vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING;
    Utils::throw_if_failed(swapchain_->Present(sync_interval, flags), "swapchain present");
}

void RendererBase::move_to_next_frame() {
    const UINT finished_frame_index = frame_index_;
    fence_values_[finished_frame_index] = graphics_queue_.signal();
    frame_index_ = swapchain_->GetCurrentBackBufferIndex();
    graphics_queue_.wait(fence_values_[frame_index_]);

    std::vector<double> passes = frame_time_.read_timestamp(frame_index_);
    passes.push_back(std::accumulate(passes.begin(), passes.end(), 0.0));
    frame_counter_.tick(passes);
}
