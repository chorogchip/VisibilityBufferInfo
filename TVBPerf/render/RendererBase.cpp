#include "RendererBase.h"

#include <string>

#include "util/Utils.h"
#include "util/GraphicsUtils.h"
#include "util/ProgramArgument.h"

using Microsoft::WRL::ComPtr;

RendererBase::~RendererBase() {
    if (command_queue_ && fence_ && fence_event_)
        wait_for_gpu();

    if (fence_event_)
        CloseHandle(fence_event_);
}

void RendererBase::init(HWND hwnd, const ProgramArgument& arg) {

    hwnd_ = hwnd;
    width_ = arg.window_width;
    height_ = arg.window_height;

    to_terminate_ = false;
    frame_count_ = 0;
    frame_warmup_count_ = arg.warmup_frames;
    frame_measure_count_ = arg.measure_frames;

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

    Utils::throw_if_failed(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "create fence");

    fence_values_[frame_index_] = 1;

    fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fence_event_)
    {
        throw std::runtime_error("CreateEvent failed");
    }

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

void RendererBase::create_root_signature() {

    // b0 (constant buffer)
    D3D12_ROOT_PARAMETER root_parameters[3]{};
    root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    root_parameters[0].Descriptor.ShaderRegister = 0;
    root_parameters[0].Descriptor.RegisterSpace = 0;
    root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // t0 (per instance structuredbuffer)
    root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    root_parameters[1].Descriptor.ShaderRegister = 0;
    root_parameters[1].Descriptor.RegisterSpace = 0;
    root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // b1 constant
    root_parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    root_parameters[2].Constants.ShaderRegister = 1;
    root_parameters[2].Constants.RegisterSpace = 0;
    root_parameters[2].Constants.Num32BitValues = 1;
    root_parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    
    D3D12_ROOT_SIGNATURE_DESC root_sig_desc{};
    root_sig_desc.NumParameters = _countof(root_parameters);
    root_sig_desc.pParameters = root_parameters;
    root_sig_desc.NumStaticSamplers = 0;
    root_sig_desc.pStaticSamplers = nullptr;
    root_sig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;

    Utils::throw_if_failed(D3D12SerializeRootSignature(
        &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error), "create root signature");

    Utils::throw_if_failed(device_->CreateRootSignature(
        0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_)), "create root signature");
}


void RendererBase::create_pso()
{
    ComPtr<ID3DBlob> vertex_shader;
    ComPtr<ID3DBlob> pixel_shader;

    GraphicsUtils::compile_shader(&vertex_shader, L"assets/shaders/forward_VS.hlsl", "vs_5_0");
    GraphicsUtils::compile_shader(&pixel_shader, L"assets/shaders/forward_PS.hlsl", "ps_5_0");

    D3D12_INPUT_ELEMENT_DESC input_layout[] =
    {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        {
            "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
            24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        }
    };

    D3D12_RASTERIZER_DESC rasterizer_desc{};
    rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer_desc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizer_desc.FrontCounterClockwise = FALSE;
    rasterizer_desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizer_desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizer_desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizer_desc.DepthClipEnable = TRUE;
    rasterizer_desc.MultisampleEnable = FALSE;
    rasterizer_desc.AntialiasedLineEnable = FALSE;
    rasterizer_desc.ForcedSampleCount = 0;
    rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_BLEND_DESC blend_desc{};
    blend_desc.AlphaToCoverageEnable = FALSE;
    blend_desc.IndependentBlendEnable = FALSE;

    const D3D12_RENDER_TARGET_BLEND_DESC default_render_target_blend_desc =
    {
        FALSE,
        FALSE,
        D3D12_BLEND_ONE,
        D3D12_BLEND_ZERO,
        D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE,
        D3D12_BLEND_ZERO,
        D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL
    };

    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
        blend_desc.RenderTarget[i] = default_render_target_blend_desc;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.InputLayout = { input_layout, _countof(input_layout) };
    pso_desc.pRootSignature = root_signature_.Get();
    pso_desc.VS = {
        vertex_shader->GetBufferPointer(),
        vertex_shader->GetBufferSize()
    };
    pso_desc.PS = {
        pixel_shader->GetBufferPointer(),
        pixel_shader->GetBufferSize()
    };
    pso_desc.RasterizerState = rasterizer_desc;
    pso_desc.BlendState = blend_desc;
    pso_desc.DepthStencilState.DepthEnable = TRUE;
    pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso_desc.DSVFormat = depth_stencil_format_;
    pso_desc.DepthStencilState.StencilEnable = FALSE;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.SampleDesc.Count = 1;

    Utils::throw_if_failed(device_->CreateGraphicsPipelineState(
        &pso_desc,
        IID_PPV_ARGS(&pipeline_state_)), "create pso");
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

    scene_raw_ = SceneSynthSphere::generate(gen_info);
    scene_resource_ = SceneSynthSphereRuntime::generate(*scene_raw_, device_.Get());
    
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

    wait_for_gpu();

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

    wait_for_gpu();
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



void RendererBase::render()
{
    Utils::throw_if_failed(command_allocator_[frame_index_]->Reset(), "reset command allocator");

    Utils::throw_if_failed(command_list_->Reset(
        command_allocator_[frame_index_].Get(),
        pipeline_state_.Get()), "command list reset on render start");

    const UINT timestamp_base = frame_index_ * GpuFrameTime<FRAME_COUNT>::TIMESTAMP_COUNT_PER_FRAME;
    const UINT timestamp_start_index = timestamp_base + GpuFrameTime<FRAME_COUNT>::TIMESTAMP_START;
    const UINT timestamp_end_index = timestamp_base + GpuFrameTime<FRAME_COUNT>::TIMESTAMP_END;

    command_list_->EndQuery(frame_time.timestamp_query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, timestamp_start_index);

    D3D12_RESOURCE_BARRIER barrier_to_rt{};
    barrier_to_rt.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_to_rt.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_to_rt.Transition.pResource = render_targets_[frame_index_].Get();
    barrier_to_rt.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier_to_rt.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier_to_rt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    command_list_->ResourceBarrier(1, &barrier_to_rt);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle =
        rtv_heap_->GetCPUDescriptorHandleForHeapStart();
    rtv_handle.ptr += static_cast<SIZE_T>(frame_index_) * rtv_descriptor_size_;
    
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
    command_list_->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);

    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissor_rect{};
    scissor_rect.left = 0;
    scissor_rect.top = 0;
    scissor_rect.right = static_cast<LONG>(width_);
    scissor_rect.bottom = static_cast<LONG>(height_);

    command_list_->RSSetViewports(1, &viewport);
    command_list_->RSSetScissorRects(1, &scissor_rect);

    const float clear_color[] = { 0.1f, 0.1f, 0.15f, 1.0f };
    command_list_->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
    command_list_->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    command_list_->SetGraphicsRootSignature(root_signature_.Get());
    command_list_->SetGraphicsRootConstantBufferView(0, buf_constant_->GetGPUVirtualAddress());
    command_list_->SetGraphicsRootShaderResourceView(1, buf_instance_->GetGPUVirtualAddress());

    command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    command_list_->IASetVertexBuffers(0, 1, &scene_resource_->vertex_buffer_view);
    command_list_->IASetIndexBuffer(&scene_resource_->index_buffer_view);
    
    int inst_num = 0;
    for (const auto& instance : scene_raw_->instances) {
        const auto& geom_handle = scene_resource_->geometries_handles[instance.geometry_id];
        
        command_list_->SetGraphicsRoot32BitConstant(2, inst_num, 0);

        command_list_->DrawIndexedInstanced(geom_handle.index_count, 1,
            geom_handle.start_index_offset, geom_handle.base_vertex_offset, 0);

        ++inst_num;
    }

    D3D12_RESOURCE_BARRIER barrier_to_present{};
    barrier_to_present.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_to_present.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_to_present.Transition.pResource = render_targets_[frame_index_].Get();
    barrier_to_present.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier_to_present.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier_to_present.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    command_list_->ResourceBarrier(1, &barrier_to_present);

    command_list_->EndQuery(frame_time.timestamp_query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, timestamp_end_index);
    command_list_->ResolveQueryData(
        frame_time.timestamp_query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
        timestamp_start_index, GpuFrameTime<FRAME_COUNT>::TIMESTAMP_COUNT_PER_FRAME,
        frame_time.timestamp_readback_buffer_.Get(), sizeof(UINT64) * timestamp_base);

    frame_time.timestamp_frame_valid_[frame_index_] = true;

    Utils::throw_if_failed(command_list_->Close(), "command list clonse on framne end");

    ID3D12CommandList* command_lists[] = { command_list_.Get() };

    command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);
    Utils::throw_if_failed(swapchain_->Present(0, DXGI_PRESENT_ALLOW_TEARING), "swapchain present");
    move_to_next_frame();
}



void RendererBase::move_to_next_frame()
{
    const UINT64 current_fence_value = fence_values_[frame_index_];

    Utils::throw_if_failed(command_queue_->Signal(
        fence_.Get(),
        current_fence_value), "command queue signal waiting next frame");

    frame_index_ = swapchain_->GetCurrentBackBufferIndex();

    if (fence_->GetCompletedValue() < fence_values_[frame_index_])
    {
        Utils::throw_if_failed(fence_->SetEventOnCompletion(
            fence_values_[frame_index_],
            fence_event_), "command queue set event on complete waiting next frame");

        WaitForSingleObject(fence_event_, INFINITE);
    }

    read_gpu_timestamp_for_frame(frame_index_);

    const double gpu_ms = frame_time.gpu_frame_ms_[frame_index_]; // TODO

    fence_values_[frame_index_] = current_fence_value + 1;

    frame_count_++;

    if (frame_count_ >= frame_warmup_count_ + frame_measure_count_ + 10)
        to_terminate_ = true;
}

void RendererBase::wait_for_gpu()
{
    Utils::throw_if_failed(command_queue_->Signal(
        fence_.Get(),
        fence_values_[frame_index_]), "command queue signal waiting gpu");

    Utils::throw_if_failed(fence_->SetEventOnCompletion(
        fence_values_[frame_index_],
        fence_event_), "command queue set event on complete waiting gpu");

    WaitForSingleObjectEx(fence_event_, INFINITE, FALSE);

    fence_values_[frame_index_]++;
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
