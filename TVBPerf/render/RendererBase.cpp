#include "RendererBase.h"

#include <string>

#include "util/Utils.h"
#include "util/GraphicsUtils.h"
#include "util/ProgramArgument.h"

#include "scene/SceneInfo.h"
#include "scene/SceneBuilder.h"
#include "scene/SceneAssimpImporter.h"
#include "scene/SceneResourceBuilder.h"

using Microsoft::WRL::ComPtr;

RendererBase::~RendererBase() {
    if (command_queue_ && fence_)
        fence_.wait_for_gpu();
}

void RendererBase::init(HWND hwnd, const ProgramArgument& arg) {

    hwnd_ = hwnd;
    width_ = arg.window_width;
    height_ = arg.window_height;

    frame_counter_.init(
        arg.warmup_frames,
        arg.warmup_frames + arg.measure_frames,
        arg.warmup_frames + arg.measure_frames + 60);

    GraphicsUtils::create_device(factory_, device_);

    GraphicsUtils::create_command_objects(device_.Get(), command_queue_, command_list_, command_allocator_, FRAME_COUNT);

    GraphicsUtils::create_swapchain(hwnd_, factory_.Get(), device_.Get(), command_queue_.Get(), swapchain_, width_, height_, FRAME_COUNT);
    frame_index_ = swapchain_->GetCurrentBackBufferIndex();

    this->create_dsv_heap();
    this->create_rtv_heap();
    this->create_render_targets();
    this->create_depth_stencil_buffer();
    this->create_root_signature();
    this->create_pso();

    fence_.init(device_.Get(), command_queue_.Get());
    fence_values_[frame_index_] = 1;

    this->create_meshbuffers(arg);
    this->create_constbuffers(arg);

    //this->create_timestamp_queries();

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

void RendererBase::create_dsv_heap()
{
    D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc{};
    dsv_heap_desc.NumDescriptors = 1;
    dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    Utils::throw_if_failed(device_->CreateDescriptorHeap(
        &dsv_heap_desc,
        IID_PPV_ARGS(&dsv_heap_)), "create descriptor heap");

    dsv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void RendererBase::create_rtv_heap() {
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
    rtv_heap_desc.NumDescriptors = FRAME_COUNT;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    Utils::throw_if_failed(device_->CreateDescriptorHeap( &rtv_heap_desc, IID_PPV_ARGS(&rtv_heap_)), "create descriptor heap");

    rtv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void RendererBase::create_render_targets() {
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < FRAME_COUNT; ++i) {
        Utils::throw_if_failed(swapchain_->GetBuffer(i, IID_PPV_ARGS(&render_targets_[i])), "create rtv");
        device_->CreateRenderTargetView(render_targets_[i].Get(), nullptr, rtv_handle);
        rtv_handle.ptr += rtv_descriptor_size_;
    }
}

void RendererBase::create_depth_stencil_buffer()
{
    D3D12_RESOURCE_DESC depth_desc{};
    depth_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depth_desc.Alignment = 0;
    depth_desc.Width = width_;
    depth_desc.Height = height_;
    depth_desc.DepthOrArraySize = 1;
    depth_desc.MipLevels = 1;
    depth_desc.Format = depth_stencil_format_;
    depth_desc.SampleDesc.Count = 1;
    depth_desc.SampleDesc.Quality = 0;
    depth_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clear_value{};
    clear_value.Format = depth_stencil_format_;
    clear_value.DepthStencil.Depth = 1.0f;
    clear_value.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES heap_props{};
    heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1;
    heap_props.VisibleNodeMask = 1;

    Utils::throw_if_failed(device_->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &depth_desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clear_value,
        IID_PPV_ARGS(&depth_stencil_buffer_)), "create depth stencil buf");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
    dsv_desc.Format = depth_stencil_format_;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
    dsv_desc.Texture2D.MipSlice = 0;

    device_->CreateDepthStencilView(
        depth_stencil_buffer_.Get(),
        &dsv_desc,
        dsv_heap_->GetCPUDescriptorHandleForHeapStart());
}

void RendererBase::create_meshbuffers(const ProgramArgument& arg)
{
    scene::SceneInfoSphere gen_info{};
    gen_info.seed = arg.seed;
    gen_info.material_count = arg.material_count;
    gen_info.mesh_count = arg.geometry_count;
    gen_info.sphere_count = arg.sphere_count;
    gen_info.z_min = arg.z_min;
    gen_info.z_max = arg.z_max;
    gen_info.xy_minmax = arg.xy_minmax;
    gen_info.radius = arg.radius;
    gen_info.mesh_division = arg.geometry_div;
    gen_info.gbuffer_resource_count = arg.gbuffer_cnt;
    
    //scene_cpu_ = scene::SceneBuilder::build(gen_info);
    scene_cpu_ = scene::SceneAssimpImporter::load(std::filesystem::current_path() /
        "assets/scenes/unpacked/SunTemple_v4/SunTemple.fbx");

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> used_upload_heaps;
    Utils::throw_if_failed(command_list_->Reset(command_allocator_[frame_index_].Get(), pipeline_state_.Get()));

    scene_gpu_ = scene::SceneResourceBuilder::build(*scene_cpu_, device_.Get(), command_list_.Get(), used_upload_heaps);

    Utils::throw_if_failed(command_list_->Close(), "close list on resource creation");
    ID3D12CommandList* command_lists[] = { command_list_.Get() };
    command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);
    fence_.wait_for_gpu();
}

void RendererBase::create_constbuffers(const ProgramArgument& arg) {

    camera_.set_pos(arg.camera_pos_x, arg.camera_pos_y, arg.camera_pos_z);
    camera_.lookat(arg.camera_lookat_x, arg.camera_lookat_y, arg.camera_lookat_z);
    camera_.set_fovy_nearz_farz(arg.camera_fov, arg.camera_near_z, arg.camera_far_z);

    DirectX::XMStoreFloat4x4(&matrix_buf_cpu_.mat_view_, DirectX::XMMatrixTranspose(
        camera_.get_mat_view()));
    DirectX::XMStoreFloat4x4(&matrix_buf_cpu_.mat_proj_, DirectX::XMMatrixTranspose(
        camera_.get_mat_proj(width_, height_)));

    constexpr size_t matrix_buf_size_aligned = Utils::GetAlignedAddress(sizeof(ConstBufMatrices), 256ULL);

    GraphicsUtils::create_buffer(buf_constant_, device_.Get(), matrix_buf_size_aligned, 1,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    
    buf_constant_mapped_ = GraphicsUtils::get_mapped_address(buf_constant_.Get());
    this->copy_camera_data();
}

void RendererBase::copy_camera_data() {
    DirectX::XMStoreFloat4x4(&matrix_buf_cpu_.mat_view_, DirectX::XMMatrixTranspose(
        camera_.get_mat_view()));
    DirectX::XMStoreFloat4x4(&matrix_buf_cpu_.mat_proj_, DirectX::XMMatrixTranspose(
        camera_.get_mat_proj(width_, height_)));
    memcpy(buf_constant_mapped_, &matrix_buf_cpu_, sizeof(matrix_buf_cpu_));
}

void RendererBase::create_timestamp_queries() {
    
    D3D12_QUERY_HEAP_DESC query_heap_desc{};
    query_heap_desc.Count = FRAME_COUNT * GpuFrameTime<FRAME_COUNT>::TIMESTAMP_COUNT_PER_FRAME;
    query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    query_heap_desc.NodeMask = 0;

    Utils::throw_if_failed(device_->CreateQueryHeap(
        &query_heap_desc,
        IID_PPV_ARGS(&frame_time.timestamp_query_heap_)),
        "create timestamp query heap");

    const UINT64 readback_buffer_size =
        sizeof(UINT64) * FRAME_COUNT * GpuFrameTime<FRAME_COUNT>::TIMESTAMP_COUNT_PER_FRAME;

    GraphicsUtils::create_buffer(frame_time.timestamp_readback_buffer_, device_.Get(), readback_buffer_size, 1,
        D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);

    Utils::throw_if_failed(command_queue_->GetTimestampFrequency(
        &frame_time.timestamp_frequency_), "get timestamp frequency");
}

void RendererBase::move_to_next_frame()
{
    const UINT finished_frame_index = frame_index_;
    fence_values_[finished_frame_index] = fence_.signal();
    frame_index_ = swapchain_->GetCurrentBackBufferIndex();
    fence_.wait_for_value(fence_values_[frame_index_]);

    //read_gpu_timestamp_for_frame(finished_frame_index);
    //const double gpu_ms = frame_time.gpu_frame_ms_[finished_frame_index]; // TODO

    frame_counter_.tick(static_cast<float>(0.0f));
}

void RendererBase::read_gpu_timestamp_for_frame(UINT frame_index)
{
    if (!frame_time.timestamp_frame_valid_[frame_index])
        return;

    const UINT timestamp_base = frame_index * GpuFrameTime<FRAME_COUNT>::TIMESTAMP_COUNT_PER_FRAME;

    UINT64* mapped_data = nullptr;

    D3D12_RANGE read_range{};
    read_range.Begin = sizeof(UINT64) * timestamp_base;
    read_range.End = sizeof(UINT64) * (timestamp_base + GpuFrameTime<FRAME_COUNT>::TIMESTAMP_COUNT_PER_FRAME);

    Utils::throw_if_failed(frame_time.timestamp_readback_buffer_->Map(
        0,
        &read_range,
        reinterpret_cast<void**>(&mapped_data)),
        "map timestamp readback");

    const UINT64 start = mapped_data[timestamp_base + 0];
    const UINT64 end = mapped_data[timestamp_base + 1];

    D3D12_RANGE write_range{};
    write_range.Begin = 0;
    write_range.End = 0;

    frame_time.timestamp_readback_buffer_->Unmap(0, &write_range);

    if (end > start && frame_time.timestamp_frequency_ != 0)
    {
        const double elapsed_ms =
            static_cast<double>(end - start) * 1000.0 /
            static_cast<double>(frame_time.timestamp_frequency_);

        frame_time.gpu_frame_ms_[frame_index] = elapsed_ms;
    }
}
