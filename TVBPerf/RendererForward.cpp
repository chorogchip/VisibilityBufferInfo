#include "RendererForward.h"

#include <string>
#include "Utils.h"
#include "GraphicsUtils.h"
#include "MeshGeometry.h"

using Microsoft::WRL::ComPtr;

void RendererForward::init(HWND hwnd, uint32_t width, uint32_t height)
{
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;

    this->create_device();
    this->create_command_objects();
    this->create_swapchain();
    this->create_dsv_heap();
    this->create_rtv_heap();
    this->create_render_targets();
    this->create_depth_stencil_buffer();
    this->create_root_signature();
    this->create_pso();
    this->create_meshbuffers();
    this->create_constbuffers();
    this->create_instancebuffers();

    Utils::throw_if_failed(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));

    fence_values_[frame_index_] = 1;

    fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fence_event_)
    {
        throw std::runtime_error("CreateEvent failed");
    }
}

void RendererForward::create_device()
{
#if defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debug_controller;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
        {
            debug_controller->EnableDebugLayer();
        }
    }
#endif

    Utils::throw_if_failed(CreateDXGIFactory1(IID_PPV_ARGS(&factory_)));

    ComPtr<IDXGIAdapter1> adapter;

    for (UINT i = 0; DXGI_ERROR_NOT_FOUND != factory_->EnumAdapters1(i, &adapter); ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        if (SUCCEEDED(D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device_))))
        {
            return;
        }
    }

    ComPtr<IDXGIAdapter> warp_adapter;
    Utils::throw_if_failed(factory_->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));

    Utils::throw_if_failed(D3D12CreateDevice(
        warp_adapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device_)));
}

void RendererForward::create_command_objects()
{
    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    Utils::throw_if_failed(device_->CreateCommandQueue(
        &queue_desc,
        IID_PPV_ARGS(&command_queue_)));

    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        Utils::throw_if_failed(device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&command_allocator_[i])));
    }

    Utils::throw_if_failed(device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_allocator_[0].Get(),
        nullptr,
        IID_PPV_ARGS(&command_list_)));

    Utils::throw_if_failed(command_list_->Close());
}

void RendererForward::create_swapchain()
{
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
    swap_chain_desc.BufferCount = FRAME_COUNT;
    swap_chain_desc.Width = width_;
    swap_chain_desc.Height = height_;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swap_chain;

    Utils::throw_if_failed(factory_->CreateSwapChainForHwnd(
        command_queue_.Get(),
        hwnd_,
        &swap_chain_desc,
        nullptr,
        nullptr,
        &swap_chain));

    Utils::throw_if_failed(factory_->MakeWindowAssociation(
        hwnd_,
        DXGI_MWA_NO_ALT_ENTER));

    Utils::throw_if_failed(swap_chain.As(&swapchain_));

    frame_index_ = swapchain_->GetCurrentBackBufferIndex();
}

void RendererForward::create_dsv_heap()
{
    D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc{};
    dsv_heap_desc.NumDescriptors = 1;
    dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    Utils::throw_if_failed(device_->CreateDescriptorHeap(
        &dsv_heap_desc,
        IID_PPV_ARGS(&dsv_heap_)));

    dsv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void RendererForward::create_rtv_heap() {
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
    rtv_heap_desc.NumDescriptors = FRAME_COUNT;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    Utils::throw_if_failed(device_->CreateDescriptorHeap( &rtv_heap_desc, IID_PPV_ARGS(&rtv_heap_)));

    rtv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void RendererForward::create_render_targets() {
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < FRAME_COUNT; ++i) {
        Utils::throw_if_failed(swapchain_->GetBuffer(i, IID_PPV_ARGS(&render_targets_[i])));
        device_->CreateRenderTargetView(render_targets_[i].Get(), nullptr, rtv_handle);
        rtv_handle.ptr += rtv_descriptor_size_;
    }
}

void RendererForward::create_depth_stencil_buffer()
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
        IID_PPV_ARGS(&depth_stencil_buffer_)));

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


void RendererForward::create_root_signature() {

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
        &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));

    Utils::throw_if_failed(device_->CreateRootSignature(
        0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_)));
}


void RendererForward::create_pso()
{
    ComPtr<ID3DBlob> vertex_shader;
    ComPtr<ID3DBlob> pixel_shader;

    GraphicsUtils::compile_shader(&vertex_shader, L"forward_VS.hlsl", "vs_5_0");
    GraphicsUtils::compile_shader(&pixel_shader, L"forward_PS.hlsl", "ps_5_0");

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
        IID_PPV_ARGS(&pipeline_state_)));
}

void RendererForward::create_meshbuffers()
{
    SceneSynthSphere::SceneInfo gen_info{};
    gen_info.seed = 0;
    gen_info.sphere_count = 10000;
    gen_info.material_count = 1;
    gen_info.geometry_count = 1;
    gen_info.z_min = -8.0f;
    gen_info.z_max = 8.0f;
    gen_info.xy_min = -5.0f;
    gen_info.xy_max = 5.0f;
    gen_info.radius_min = 0.05f;
    gen_info.radius_max = 0.2f;
    gen_info.geometry_division_min = 1;
    gen_info.geometry_division_max = 20;
    gen_info.material_float4_count = 1;

    scene_raw_ = SceneSynthSphere::generate(gen_info);
    scene_resource_ = SceneSynthSphereRuntime::generate(*scene_raw_, device_.Get());
}

void RendererForward::create_constbuffers() {

    const auto vec_eye = DirectX::XMVectorSet(0.0f, 0.0f, -10.0f, 0.0f);
    const auto vec_target = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    const auto vec_up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    const auto mat_v{ DirectX::XMMatrixLookAtLH(vec_eye, vec_target, vec_up) };

    constexpr float fov_y = DirectX::XMConvertToRadians(45.0f);
    const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    constexpr float near_z = 0.01f;
    constexpr float far_z = 1000.0f;

    const auto mat_p{ DirectX::XMMatrixPerspectiveFovLH(fov_y, aspect, near_z, far_z) };

    DirectX::XMStoreFloat4x4(&matrix_buf_cpu_.mat_view_, DirectX::XMMatrixTranspose(mat_v));
    DirectX::XMStoreFloat4x4(&matrix_buf_cpu_.mat_proj_, DirectX::XMMatrixTranspose(mat_p));

    constexpr size_t matrix_buf_size = Utils::GetAlignedAddress(sizeof(ConstBufMatrices), 256ULL);

    D3D12_HEAP_PROPERTIES heap_props{};
    heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;  // TODO switch to DEFAULT
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1;
    heap_props.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Width = matrix_buf_size;
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    Utils::throw_if_failed(device_->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buf_constant_)));

    void* mappedData = nullptr;

    D3D12_RANGE readRange = {};
    readRange.Begin = 0;
    readRange.End = 0;
    D3D12_RANGE writtenRange = {};
    writtenRange.Begin = 0;
    writtenRange.End = sizeof(matrix_buf_cpu_);

    Utils::throw_if_failed(buf_constant_->Map(0, &readRange, &mappedData));
    memcpy(mappedData, &matrix_buf_cpu_, sizeof(matrix_buf_cpu_));
    buf_constant_->Unmap(0, &writtenRange);
}

void RendererForward::create_instancebuffers() {

    const size_t total_sz = scene_resource_->instances_datas.size() * sizeof(scene_resource_->instances_datas[0]);
    assert(total_sz > 0);

    D3D12_HEAP_PROPERTIES heap_props{};
    heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;  // TODO switch to DEFAULT
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1;
    heap_props.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Alignment = 0;
    resource_desc.Width = total_sz;
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    Utils::throw_if_failed(device_->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buf_instance_)));

    void* mappedData = nullptr;

    D3D12_RANGE readRange = {};
    readRange.Begin = 0;
    readRange.End = 0;
    D3D12_RANGE writtenRange = {};
    writtenRange.Begin = 0;
    writtenRange.End = total_sz;

    Utils::throw_if_failed(buf_instance_->Map(0, &readRange, &mappedData));
    memcpy(mappedData, scene_resource_->instances_datas.data(), total_sz);
    buf_instance_->Unmap(0, &writtenRange);
}

void RendererForward::render()
{
    Utils::throw_if_failed(command_allocator_[frame_index_]->Reset());

    Utils::throw_if_failed(command_list_->Reset(
        command_allocator_[frame_index_].Get(),
        pipeline_state_.Get()));

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

    Utils::throw_if_failed(command_list_->Close());

    ID3D12CommandList* command_lists[] = { command_list_.Get() };

    command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);
    Utils::throw_if_failed(swapchain_->Present(1, 0));
    move_to_next_frame();
}

void RendererForward::move_to_next_frame()
{
    const UINT64 current_fence_value = fence_values_[frame_index_];

    Utils::throw_if_failed(command_queue_->Signal(
        fence_.Get(),
        current_fence_value));

    frame_index_ = swapchain_->GetCurrentBackBufferIndex();

    if (fence_->GetCompletedValue() < fence_values_[frame_index_])
    {
        Utils::throw_if_failed(fence_->SetEventOnCompletion(
            fence_values_[frame_index_],
            fence_event_));

        WaitForSingleObject(fence_event_, INFINITE);
    }

    fence_values_[frame_index_] = current_fence_value + 1;
}

void RendererForward::wait_for_gpu()
{
    Utils::throw_if_failed(command_queue_->Signal(
        fence_.Get(),
        fence_values_[frame_index_]));

    Utils::throw_if_failed(fence_->SetEventOnCompletion(
        fence_values_[frame_index_],
        fence_event_));

    WaitForSingleObjectEx(fence_event_, INFINITE, FALSE);

    fence_values_[frame_index_]++;
}