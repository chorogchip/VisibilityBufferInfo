#include "RendererBase.h"

#include <string>

#include "util/GraphicsUtils.h"
#include "util/Utils.h"

#include "scene/SceneAssimpImporter.h"
#include "scene/SceneBuilder.h"
#include "scene/SceneInfo.h"
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

    program_arguments_ = std::unique_ptr<const ProgramArgument>{ new ProgramArgument{ arg } };


    frame_counter_.init(arg.warmup_frames, arg.warmup_frames + arg.measure_frames,
        arg.warmup_frames + arg.measure_frames + 60);

    this->create_device();
    this->create_command_objects();
    this->create_swapchain();

    fence_.init(device_.Get(), command_queue_.Get());
    fence_values_[frame_index_] = 1;

    this->init_viewport_scissorrect();
    this->init_();

    this->create_meshbuffers();
    this->create_constbuffers();

    this->create_dsv_heap();
    this->create_depth_stencil_buffer();
    this->create_rtv_heap();
    this->create_render_targets();
    this->create_srv_heap();
    this->create_shader_resources();

    this->create_root_signature();
    this->create_pso();



    // this->create_timestamp_queries();
}

void RendererBase::render() {

    this->render_();

    this->move_to_next_frame();

}

void RendererBase::create_device() {
#if defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debug_controller;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
            debug_controller->EnableDebugLayer();
        }
    }
#endif

Utils::throw_if_failed(CreateDXGIFactory1(IID_PPV_ARGS(&factory_)), "create DXGI factory");

ComPtr<IDXGIAdapter1> adapter;

for (UINT i = 0; DXGI_ERROR_NOT_FOUND != factory_->EnumAdapters1(i, &adapter); ++i) {
    DXGI_ADAPTER_DESC1 desc;
    adapter->GetDesc1(&desc);

    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        continue;

    if (SUCCEEDED(D3D12CreateDevice(
        adapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device_)))) {
        return;
    }
}

ComPtr<IDXGIAdapter> warp_adapter;
Utils::throw_if_failed(factory_->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)), "enumerate adapter");

Utils::throw_if_failed(D3D12CreateDevice(
    warp_adapter.Get(),
    D3D_FEATURE_LEVEL_11_0,
    IID_PPV_ARGS(&device_)), "create device");
}

void RendererBase::create_command_objects() {

    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    Utils::throw_if_failed(device_->CreateCommandQueue(
        &queue_desc,
        IID_PPV_ARGS(&command_queue_)), "create command queue");

    for (int i = 0; i < FRAME_COUNT; ++i) {
        Utils::throw_if_failed(device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&command_allocator_[i])), "create command allocator");
    }

    Utils::throw_if_failed(device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_allocator_[0].Get(),
        nullptr,
        IID_PPV_ARGS(&command_list_)), "create command list");

    Utils::throw_if_failed(command_list_->Close(), "command list close");
}

void RendererBase::create_swapchain() {

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
    swap_chain_desc.BufferCount = FRAME_COUNT;
    swap_chain_desc.Width = width_;
    swap_chain_desc.Height = height_;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGISwapChain1> swap_chain;

    Utils::throw_if_failed(factory_->CreateSwapChainForHwnd(
        command_queue_.Get(),
        hwnd_,
        &swap_chain_desc,
        nullptr,
        nullptr,
        &swap_chain), "create swapchain");

    Utils::throw_if_failed(factory_->MakeWindowAssociation(
        hwnd_,
        DXGI_MWA_NO_ALT_ENTER), "factory make window association");

    Utils::throw_if_failed(swap_chain.As(&swapchain_), "swapchain as");


    frame_index_ = swapchain_->GetCurrentBackBufferIndex();
}


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

void RendererBase::create_srv_heap() {}
void RendererBase::create_shader_resources() {}

void RendererBase::create_meshbuffers() {

    if (program_arguments_->to_use_scene) {
        scene_cpu_ = scene::SceneAssimpImporter::load(
            std::filesystem::current_path() /
            "assets/scenes/unpacked/SunTemple_v4/SunTemple.fbx");
    } else {
        scene::SceneInfoSphere gen_info{};
        gen_info.seed = program_arguments_->seed;
        gen_info.material_count = program_arguments_->material_count;
        gen_info.mesh_count = program_arguments_->geometry_count;
        gen_info.sphere_count = program_arguments_->sphere_count;
        gen_info.z_min = program_arguments_->z_min;
        gen_info.z_max = program_arguments_->z_max;
        gen_info.xy_minmax = program_arguments_->xy_minmax;
        gen_info.radius = program_arguments_->radius;
        gen_info.mesh_division = program_arguments_->geometry_div;
        gen_info.gbuffer_resource_count = program_arguments_->gbuffer_cnt;

        scene_cpu_ = scene::SceneBuilder::build(gen_info);
    }

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

    constexpr size_t matrix_buf_size_aligned =
        Utils::GetAlignedAddress(sizeof(ConstBufMatrices), 256ULL);

    for (int i = 0; i < FRAME_COUNT; ++i) {
        GraphicsUtils::create_buffer(
            buf_constant_[i], device_.Get(), matrix_buf_size_aligned, 1,
            D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

        buf_constant_mapped_[i] = GraphicsUtils::get_mapped_address(buf_constant_[i].Get());
    }
}

void RendererBase::copy_camera_data() {
    DirectX::XMStoreFloat4x4(&matrix_buf_cpu_.mat_view_,
        DirectX::XMMatrixTranspose(camera_.get_mat_view()));
    DirectX::XMStoreFloat4x4(
        &matrix_buf_cpu_.mat_proj_,
        DirectX::XMMatrixTranspose(camera_.get_mat_proj(width_, height_)));
    memcpy(buf_constant_mapped_[frame_index_], &matrix_buf_cpu_, sizeof(matrix_buf_cpu_));
}

void RendererBase::create_timestamp_queries() {

    D3D12_QUERY_HEAP_DESC query_heap_desc{};
    query_heap_desc.Count =
        FRAME_COUNT * GpuFrameTime<FRAME_COUNT>::TIMESTAMP_COUNT_PER_FRAME;
    query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    query_heap_desc.NodeMask = 0;

    Utils::throw_if_failed(
        device_->CreateQueryHeap(&query_heap_desc,
            IID_PPV_ARGS(&frame_time.timestamp_query_heap_)),
        "create timestamp query heap");

    const UINT64 readback_buffer_size =
        sizeof(UINT64) * FRAME_COUNT *
        GpuFrameTime<FRAME_COUNT>::TIMESTAMP_COUNT_PER_FRAME;

    GraphicsUtils::create_buffer(frame_time.timestamp_readback_buffer_,
        device_.Get(), readback_buffer_size, 1,
        D3D12_HEAP_TYPE_READBACK,
        D3D12_RESOURCE_STATE_COPY_DEST);

    Utils::throw_if_failed(
        command_queue_->GetTimestampFrequency(&frame_time.timestamp_frequency_),
        "get timestamp frequency");
}

void RendererBase::move_to_next_frame() {
    const UINT finished_frame_index = frame_index_;
    fence_values_[finished_frame_index] = fence_.signal();
    frame_index_ = swapchain_->GetCurrentBackBufferIndex();
    fence_.wait_for_value(fence_values_[frame_index_]);

    // read_gpu_timestamp_for_frame(finished_frame_index);
    // const double gpu_ms = frame_time.gpu_frame_ms_[finished_frame_index]; //
    // TODO

    frame_counter_.tick(static_cast<float>(0.0f));
}

void RendererBase::read_gpu_timestamp_for_frame(UINT frame_index) {
    if (!frame_time.timestamp_frame_valid_[frame_index])
        return;

    const UINT timestamp_base =
        frame_index * GpuFrameTime<FRAME_COUNT>::TIMESTAMP_COUNT_PER_FRAME;

    UINT64* mapped_data = nullptr;

    D3D12_RANGE read_range{};
    read_range.Begin = sizeof(UINT64) * timestamp_base;
    read_range.End =
        sizeof(UINT64) *
        (timestamp_base + GpuFrameTime<FRAME_COUNT>::TIMESTAMP_COUNT_PER_FRAME);

    Utils::throw_if_failed(
        frame_time.timestamp_readback_buffer_->Map(
            0, &read_range, reinterpret_cast<void**>(&mapped_data)),
        "map timestamp readback");

    const UINT64 start = mapped_data[timestamp_base + 0];
    const UINT64 end = mapped_data[timestamp_base + 1];

    D3D12_RANGE write_range{};
    write_range.Begin = 0;
    write_range.End = 0;

    frame_time.timestamp_readback_buffer_->Unmap(0, &write_range);

    if (end > start && frame_time.timestamp_frequency_ != 0) {
        const double elapsed_ms =
            static_cast<double>(end - start) * 1000.0 /
            static_cast<double>(frame_time.timestamp_frequency_);

        frame_time.gpu_frame_ms_[frame_index] = elapsed_ms;
    }
}
