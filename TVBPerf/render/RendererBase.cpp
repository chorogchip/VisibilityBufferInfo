#include "RendererBase.h"

#include <string>

#include "util/Utils.h"
#include "util/GraphicsUtils.h"
#include "util/ProgramArgument.h"
#include "util/SceneAssimpImporter.h"

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
    this->create_instancebuffers();

    this->create_timestamp_queries();
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
    SceneSynthSphere::SceneInfo gen_info{};
    gen_info.seed = arg.seed;
    gen_info.sphere_count = arg.sphere_count;
    gen_info.material_count = arg.material_count;
    gen_info.geometry_count = arg.geometry_count;
    gen_info.z_min = arg.z_min;
    gen_info.z_max = arg.z_max;
    gen_info.xy_min = arg.xy_min;
    gen_info.xy_max = arg.xy_max;
    gen_info.radius_min = arg.radius_min;
    gen_info.radius_max = arg.radius_max;
    gen_info.geometry_division_min = arg.geometry_div_min;
    gen_info.geometry_division_max = arg.geometry_div_max;
    gen_info.material_float4_count = arg.gbuffer_cnt;


    auto imported_scene = load_imported_scene_with_assimp(
        std::filesystem::current_path() / "assets/scenes/unpacked/hairball/hairball.obj");
    scene_resource_ = SceneSynthSphereRuntime::generate(*imported_scene, device_.Get());

    //scene_raw_ = SceneSynthSphere::generate(gen_info);
    //scene_resource_ = SceneSynthSphereRuntime::generate(*scene_raw_, device_.Get());
    
    ComPtr<ID3D12Resource> vb_default, ib_default;
    GraphicsUtils::create_buffer(vb_default, device_.Get(), scene_resource_->vertex_buffer_size, 1,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
    GraphicsUtils::create_buffer(ib_default, device_.Get(), scene_resource_->index_buffer_size, 1,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);

    Utils::throw_if_failed(command_list_->Reset(command_allocator_[frame_index_].Get(), pipeline_state_.Get()));
    command_list_->CopyBufferRegion(vb_default.Get(), 0,
        scene_resource_->vertex_buffer.Get(), 0, scene_resource_->vertex_buffer_size);
    command_list_->CopyBufferRegion(ib_default.Get(), 0,
        scene_resource_->index_buffer.Get(), 0, scene_resource_->index_buffer_size);

    D3D12_RESOURCE_BARRIER barrier[2]{};
    barrier[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier[0].Transition.pResource = vb_default.Get();
    barrier[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier[0].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrier[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier[1].Transition.pResource = ib_default.Get();
    barrier[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier[1].Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;

    command_list_->ResourceBarrier(2, barrier);
    Utils::throw_if_failed(command_list_->Close(), "cpy");

    ID3D12CommandList* command_lists[] = { command_list_.Get() };
    command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);

    fence_.wait_for_gpu();

    using Vertex = decltype(scene_raw_->geometries[0].vertices)::value_type;
    using Index = decltype(scene_raw_->geometries[0].indices)::value_type;

    scene_resource_->vertex_buffer_view.BufferLocation = vb_default->GetGPUVirtualAddress();
    scene_resource_->vertex_buffer_view.StrideInBytes = sizeof(Vertex);
    assert(scene_resource_->vertex_buffer_size < static_cast<size_t>(UINT_MAX));
    scene_resource_->vertex_buffer_view.SizeInBytes = static_cast<UINT>(scene_resource_->vertex_buffer_size);

    scene_resource_->index_buffer_view.BufferLocation = ib_default->GetGPUVirtualAddress();
    scene_resource_->index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
    assert(scene_resource_->index_buffer_size < static_cast<size_t>(UINT_MAX));
    scene_resource_->index_buffer_view.SizeInBytes = static_cast<UINT>(scene_resource_->index_buffer_size);

    scene_resource_->vertex_buffer = std::move(vb_default);
    scene_resource_->index_buffer = std::move(ib_default);
    
}

void RendererBase::create_constbuffers(const ProgramArgument& arg) {

    const auto vec_eye = DirectX::XMVectorSet(arg.camera_pos_x, arg.camera_pos_y, arg.camera_pos_z, 0.0f);
    const auto vec_target = DirectX::XMVectorSet(arg.camera_lookat_x, arg.camera_lookat_y, arg.camera_lookat_z, 0.0f);
    const auto vec_up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    const auto mat_v{ DirectX::XMMatrixLookAtLH(vec_eye, vec_target, vec_up) };

    const float fov_y = DirectX::XMConvertToRadians(arg.camera_fov);
    const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    const float near_z = arg.camera_near_z;
    const float far_z = arg.camera_far_z;

    const auto mat_p{ DirectX::XMMatrixPerspectiveFovLH(fov_y, aspect, near_z, far_z) };

    DirectX::XMStoreFloat4x4(&matrix_buf_cpu_.mat_view_, DirectX::XMMatrixTranspose(mat_v));
    DirectX::XMStoreFloat4x4(&matrix_buf_cpu_.mat_proj_, DirectX::XMMatrixTranspose(mat_p));

    constexpr size_t matrix_buf_size_aligned = Utils::GetAlignedAddress(sizeof(ConstBufMatrices), 256ULL);

    GraphicsUtils::create_buffer(buf_constant_, device_.Get(), matrix_buf_size_aligned, 1,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

    GraphicsUtils::copy_cpu_to_upload(buf_constant_.Get(), &matrix_buf_cpu_, sizeof(matrix_buf_cpu_));
}

void RendererBase::create_instancebuffers() {

    ComPtr<ID3D12Resource> buf_instance_upload;
    const size_t total_sz = scene_resource_->instances_datas.size() * sizeof(scene_resource_->instances_datas[0]);
    assert(total_sz > 0);

    GraphicsUtils::create_buffer(buf_instance_upload, device_.Get(), total_sz, 1,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

    GraphicsUtils::create_buffer(buf_instance_, device_.Get(), total_sz, 1,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);

    GraphicsUtils::copy_cpu_to_upload(buf_instance_upload.Get(), scene_resource_->instances_datas.data(), total_sz);

    Utils::throw_if_failed(command_list_->Reset(
        command_allocator_[frame_index_].Get(),
        pipeline_state_.Get()), "command list reset on indeg buffer gen");
    command_list_->CopyBufferRegion(buf_instance_.Get(), 0, buf_instance_upload.Get(), 0, total_sz);
    
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = buf_instance_.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    command_list_->ResourceBarrier(1, &barrier);
    Utils::throw_if_failed(command_list_->Close(), "close instance upload command list");

    ID3D12CommandList* command_lists[] = { command_list_.Get() };
    command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);

    fence_.wait_for_gpu();
    buf_instance_upload = nullptr;
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
