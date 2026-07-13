#include "render/RendererBase.h"

#include <algorithm>
#include <numeric>
#include <string>

#include "util/Utils.h"
#include "util/BenchmarkCsvWriter.h"
#include "dx_util/DeviceUtils.h"
#include "dx_util/DescriptorUtils.h"
#include "dx_util/ResourceUtils.h"

#include "scene/SceneFingerprint.h"
#include "scene/SceneLoader.h"
#include "scene/SceneResourceBuilder.h"

#ifdef max
#undef max
#undef min
#endif

using Microsoft::WRL::ComPtr;

RendererBase::~RendererBase() {
    if (command_queue_ && fence_)
        fence_.wait_for_gpu();
}

void RendererBase::init(HWND hwnd, const util::ProgramArgument& arg) {

    hwnd_ = hwnd;
    width_ = arg.window_width;
    height_ = arg.window_height;

    program_arguments_ = std::make_unique<const util::ProgramArgument>(arg);

    frame_counter_.init(dxutl::GpuFrameTimer::PASS_COUNT + 1, arg.warmup_frames, arg.warmup_frames + arg.measure_frames,
        arg.warmup_frames + arg.measure_frames + 60);

    device_ = dxutl::create_device(factory_);
    this->create_command_objects();
    swapchain_ = dxutl::create_swapchain(
        factory_.Get(), command_queue_.Get(), hwnd_, width_, height_, FRAME_COUNT);
    frame_index_ = swapchain_->GetCurrentBackBufferIndex();

    fence_.init(device_.Get(), command_queue_.Get());
    fence_values_[frame_index_] = 1;
    frame_time_.init(device_.Get(), command_queue_.Get());

    this->init_viewport_scissorrect();

    // Pass resources must be created before descriptor counts and views are configured.
    this->create_pass_resources();

    this->create_meshbuffers();
    this->create_dummy_textures();
    this->create_constbuffers();

    this->create_depth_stencil_resources();
    this->create_render_target_views();
    this->create_shader_visible_srv_heap();
    this->create_sampler_heap();
    this->create_texture_sampler_descriptors();
    this->create_shader_resources();

    this->create_root_signature();
    this->create_pso();
}

void RendererBase::render() {

    this->render_();

    this->move_to_next_frame();
}

void RendererBase::close() {
    fence_.wait_for_gpu();

    const std::string& path = program_arguments_->output_filepath;
    if (path == "") return;

    util::ProgramResult result{};
    auto results = frame_counter_.summarize();

#define COPY_PASS_RESULT(index) \
    if (results.size() > index) { \
        result.pass_##index##_time_min_ms = results[index].time_min_ms; \
        result.pass_##index##_time_median_ms = results[index].time_median_ms; \
        result.pass_##index##_time_max_ms = results[index].time_max_ms; \
        result.pass_##index##_time_avg_ms = results[index].time_avg_ms; \
        result.pass_##index##_time_p01_ms = results[index].time_p01_ms; \
        result.pass_##index##_time_p10_ms = results[index].time_p10_ms; \
        result.pass_##index##_time_p90_ms = results[index].time_p90_ms; \
        result.pass_##index##_time_p99_ms = results[index].time_p99_ms; \
    }
    COPY_PASS_RESULT(0)
    COPY_PASS_RESULT(1)
    COPY_PASS_RESULT(2)
    COPY_PASS_RESULT(3)
#undef COPY_PASS_RESULT

    this->make_programresult(result);

    util::write_benchmark_csv(path, *program_arguments_, result);
}

void RendererBase::create_command_objects() {
    command_queue_ = dxutl::create_command_queue(device_.Get());

    for (int i = 0; i < FRAME_COUNT; ++i) {
        Utils::throw_if_failed(device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(command_allocator_[i].ReleaseAndGetAddressOf())), "create command allocator");
    }

    Utils::throw_if_failed(device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_allocator_[0].Get(),
        nullptr,
        IID_PPV_ARGS(command_list_.ReleaseAndGetAddressOf())), "create command list");

    Utils::throw_if_failed(command_list_->Close(), "command list close");
}

void RendererBase::create_pass_resources() {}

void RendererBase::init_viewport_scissorrect() {
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

UINT RendererBase::dsv_descriptor_count() const {
    return 1;
}

D3D12_RESOURCE_STATES RendererBase::depth_stencil_initial_state() const {
    return D3D12_RESOURCE_STATE_DEPTH_WRITE;
}

void RendererBase::create_extra_depth_stencil_views() {}

UINT RendererBase::rtv_descriptor_count() const {
    return FRAME_COUNT;
}

void RendererBase::create_extra_render_target_views(D3D12_CPU_DESCRIPTOR_HANDLE) {}

UINT RendererBase::srv_descriptor_count() const {
    return 0;
}

void RendererBase::create_shader_resources() {}

void RendererBase::create_depth_stencil_resources() {
    dsv_heap_ = dxutl::create_descriptor_heap(
        device_.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        dsv_descriptor_count(),
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    dsv_descriptor_size_ = dxutl::descriptor_size(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    depth_stencil_buffer_ = dxutl::create_depth_stencil_buffer(
        device_.Get(),
        width_,
        height_,
        DEPTH_STENCIL_FORMAT_,
        depth_stencil_initial_state());

    dxutl::create_depth_stencil_view(
        device_.Get(),
        depth_stencil_buffer_.Get(),
        DEPTH_STENCIL_FORMAT_,
        dsv_heap_->GetCPUDescriptorHandleForHeapStart());

    create_extra_depth_stencil_views();
}

void RendererBase::create_render_target_views() {
    rtv_heap_ = dxutl::create_descriptor_heap(
        device_.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        rtv_descriptor_count(),
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    rtv_descriptor_size_ = dxutl::descriptor_size(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    dxutl::create_swapchain_render_target_views(
        device_.Get(), swapchain_.Get(), rtv_heap_.Get(), rtv_descriptor_size_, FRAME_COUNT, render_targets_);

    create_extra_render_target_views(dxutl::offset_cpu_descriptor(
        rtv_heap_->GetCPUDescriptorHandleForHeapStart(), rtv_descriptor_size_, FRAME_COUNT));
}

void RendererBase::create_shader_visible_srv_heap() {
    const UINT descriptor_count = srv_descriptor_count();
    if (descriptor_count == 0) return;

    srv_heap_ = dxutl::create_descriptor_heap(
        device_.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        descriptor_count,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        "create srv descriptor heap");
    srv_descriptor_size_ = dxutl::descriptor_size(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void RendererBase::create_sampler_heap() {
    const UINT sampler_count = program_arguments_->texture_count;

    util::Logger::g_logger.assert_with_log(
        sampler_count > 0,
        "sampler count must > 0");

    sampler_heap_ = dxutl::create_descriptor_heap(
        device_.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        sampler_count,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        "create sampler descriptor heap");

    sampler_descriptor_size_ = dxutl::descriptor_size(
        device_.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
    );
}

void RendererBase::create_texture_sampler_descriptors() {
    D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle =
        sampler_heap_->GetCPUDescriptorHandleForHeapStart();

    const UINT sampler_count = program_arguments_->texture_count;

    for (UINT i = 0; i < sampler_count; ++i) {
        D3D12_SAMPLER_DESC sampler_desc{};

        sampler_desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

        sampler_desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

        sampler_desc.MipLODBias = 0.0f;
        sampler_desc.MaxAnisotropy = 1;
        sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

        sampler_desc.BorderColor[0] = 0.0f;
        sampler_desc.BorderColor[1] = 0.0f;
        sampler_desc.BorderColor[2] = 0.0f;
        sampler_desc.BorderColor[3] = 0.0f;

        sampler_desc.MinLOD = 0.0f;
        sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;

        device_->CreateSampler(&sampler_desc, sampler_handle);

        sampler_handle.ptr += sampler_descriptor_size_;
    }
}

void RendererBase::create_meshbuffers() {

    scene_cpu_ = scene::SceneLoader::load(*program_arguments_);
    scene::SceneFingerprint::write_csv(
        util::get_scene_fingerprint_output_path(program_arguments_->output_filepath),
        *scene_cpu_,
        *program_arguments_);

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> used_upload_heaps;
    Utils::throw_if_failed(command_list_->Reset(
        command_allocator_[frame_index_].Get(), pipeline_state_.Get()));

    scene_gpu_ = scene::SceneResourceBuilder::build(
        *scene_cpu_, device_.Get(), command_list_.Get(), used_upload_heaps);

    Utils::throw_if_failed(command_list_->Close(),
        "close list on resource creation");

    ID3D12CommandList* command_lists[] = { command_list_.Get() };
    command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);
    fence_.wait_for_gpu();
}

void RendererBase::create_dummy_textures() {

    const UINT texture_count = program_arguments_->texture_count;
    const UINT texture_size = program_arguments_->texture_size;

    util::Logger::g_logger.assert_with_log(texture_count > 0, "texture count must > 0");
    util::Logger::g_logger.assert_with_log(texture_size > 0, "texture size must > 0");
    const DXGI_FORMAT texture_format = DXGI_FORMAT_R8G8B8A8_UNORM;

    dummy_textures_.clear();
    dummy_textures_.resize(texture_count);

    D3D12_RESOURCE_DESC texture_desc{};
    texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_desc.Alignment = 0;
    texture_desc.Width = texture_size;
    texture_desc.Height = texture_size;
    texture_desc.DepthOrArraySize = 1;
    texture_desc.MipLevels = 1;
    texture_desc.Format = texture_format;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    UINT64 upload_size = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT row_count = 0;
    UINT64 row_size_in_bytes = 0;
    device_->GetCopyableFootprints(
        &texture_desc, 0, 1, 0, &footprint, &row_count, &row_size_in_bytes, &upload_size);
    (void)row_count;
    (void)row_size_in_bytes;

    std::vector<ComPtr<ID3D12Resource>> upload_buffers;
    upload_buffers.resize(texture_count);

    Utils::throw_if_failed(command_list_->Reset(
        command_allocator_[frame_index_].Get(), pipeline_state_.Get()));

    for (UINT texture_index = 0; texture_index < texture_count; ++texture_index) {
        dummy_textures_[texture_index] = dxutl::create_committed_resource(
            device_.Get(),
            texture_desc,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_COPY_DEST);

        upload_buffers[texture_index] = dxutl::create_buffer(device_.Get(), upload_size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

        void* mapped_data = dxutl::map_upload_buffer(upload_buffers[texture_index].Get());
        auto* dst = static_cast<uint8_t*>(mapped_data);
        for (UINT y = 0; y < texture_size; ++y) {
            auto* row = dst + footprint.Offset + static_cast<size_t>(y) * footprint.Footprint.RowPitch;
            for (UINT x = 0; x < texture_size; ++x) {
                const size_t texel_offset = static_cast<size_t>(x) * 4;
                row[texel_offset + 0] = static_cast<uint8_t>((x * 17 + texture_index * 29) & 0xff);
                row[texel_offset + 1] = static_cast<uint8_t>((y * 19 + texture_index * 31) & 0xff);
                row[texel_offset + 2] = static_cast<uint8_t>(((x + y) * 13 + texture_index * 37) & 0xff);
                row[texel_offset + 3] = 255;
            }
        }
        upload_buffers[texture_index]->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION dst_location{};
        dst_location.pResource = dummy_textures_[texture_index].Get();
        dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst_location.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src_location{};
        src_location.pResource = upload_buffers[texture_index].Get();
        src_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src_location.PlacedFootprint = footprint;

        command_list_->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);
        dxutl::transition_resource(command_list_.Get(), dummy_textures_[texture_index].Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    Utils::throw_if_failed(command_list_->Close(),
        "close list on dummy texture creation");
    ID3D12CommandList* command_lists[] = { command_list_.Get() };
    command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);
    fence_.wait_for_gpu();
}

void RendererBase::create_constbuffers() {

    camera_.set_pos(program_arguments_->camera_pos_x, program_arguments_->camera_pos_y, program_arguments_->camera_pos_z);
    camera_.lookat(program_arguments_->camera_lookat_x, program_arguments_->camera_lookat_y, program_arguments_->camera_lookat_z);
    camera_.set_fovy_nearz_farz(program_arguments_->camera_fov, program_arguments_->camera_near_z,
        program_arguments_->camera_far_z);

    DirectX::XMStoreFloat4x4(&matrix_buf_cpu_.mat_view_,
        DirectX::XMMatrixTranspose(camera_.get_mat_view()));
    DirectX::XMStoreFloat4x4(
        &matrix_buf_cpu_.mat_proj_,
        DirectX::XMMatrixTranspose(camera_.get_mat_proj(width_, height_)));
    matrix_buf_cpu_.viewport_size_ = DirectX::XMFLOAT2(static_cast<float>(width_), static_cast<float>(height_));

    constexpr size_t matrix_buf_size_aligned =
        Utils::GetAlignedAddress(sizeof(ConstBufMatrices), 256ULL);

    for (int i = 0; i < FRAME_COUNT; ++i) {
        buf_constant_[i] = dxutl::create_buffer(device_.Get(), matrix_buf_size_aligned, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

        buf_constant_mapped_[i] = dxutl::map_upload_buffer(buf_constant_[i].Get());
    }
}

void RendererBase::copy_camera_data() {

    DirectX::XMStoreFloat4x4(&matrix_buf_cpu_.mat_view_,
        DirectX::XMMatrixTranspose(camera_.get_mat_view()));
    DirectX::XMStoreFloat4x4(
        &matrix_buf_cpu_.mat_proj_,
        DirectX::XMMatrixTranspose(camera_.get_mat_proj(width_, height_)));

    matrix_buf_cpu_.viewport_size_ =
        DirectX::XMFLOAT2(static_cast<float>(width_), static_cast<float>(height_));

    memcpy(buf_constant_mapped_[frame_index_], &matrix_buf_cpu_, sizeof(matrix_buf_cpu_));
}

void RendererBase::create_texture_srv_descriptors(D3D12_CPU_DESCRIPTOR_HANDLE srv_handle) {

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;

    for (UINT i = 0; i < program_arguments_->texture_count; ++i) {
        device_->CreateShaderResourceView(dummy_textures_[i].Get(), &srv_desc, srv_handle);
        srv_handle.ptr += device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
}

void RendererBase::move_to_next_frame() {
    const UINT finished_frame_index = frame_index_;
    fence_values_[finished_frame_index] = fence_.signal();
    frame_index_ = swapchain_->GetCurrentBackBufferIndex();
    fence_.wait_for_value(fence_values_[frame_index_]);

    std::vector<double> passes = frame_time_.read_timestamp(frame_index_);
    passes.push_back(std::accumulate(passes.begin(), passes.end(), 0.0));
    frame_counter_.tick(passes);
}
