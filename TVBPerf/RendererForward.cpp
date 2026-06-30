#include "RendererForward.h"

#include <string>
#include "Utils.h"
#include "GraphicsUtils.h"
#include "MeshGeometry.h"

using Microsoft::WRL::ComPtr;

void RendererForward::renderer_init(HWND hwnd, uint32_t width, uint32_t height)
{
    m_hwnd = hwnd;
    m_width = width;
    m_height = height;

    this->create_device();
    this->create_command_objects();
    this->create_swapchain();
    this->create_rtv_heap();
    this->create_render_targets();
    this->create_root_signature();
    this->create_pso();
    this->create_meshbuffers();

    Utils::throw_if_failed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

    m_fence_values[m_frame_index] = 1;

    m_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fence_event)
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

    Utils::throw_if_failed(CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)));

    ComPtr<IDXGIAdapter1> adapter;

    for (UINT i = 0; DXGI_ERROR_NOT_FOUND != m_factory->EnumAdapters1(i, &adapter); ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        if (SUCCEEDED(D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device))))
        {
            return;
        }
    }

    ComPtr<IDXGIAdapter> warp_adapter;
    Utils::throw_if_failed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));

    Utils::throw_if_failed(D3D12CreateDevice(
        warp_adapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_device)));
}

void RendererForward::create_command_objects()
{
    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    Utils::throw_if_failed(m_device->CreateCommandQueue(
        &queue_desc,
        IID_PPV_ARGS(&m_command_queue)));

    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        Utils::throw_if_failed(m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_command_allocator[i])));
    }

    Utils::throw_if_failed(m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_command_allocator[0].Get(),
        nullptr,
        IID_PPV_ARGS(&m_command_list)));

    Utils::throw_if_failed(m_command_list->Close());
}

void RendererForward::create_swapchain()
{
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
    swap_chain_desc.BufferCount = FRAME_COUNT;
    swap_chain_desc.Width = m_width;
    swap_chain_desc.Height = m_height;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swap_chain;

    Utils::throw_if_failed(m_factory->CreateSwapChainForHwnd(
        m_command_queue.Get(),
        m_hwnd,
        &swap_chain_desc,
        nullptr,
        nullptr,
        &swap_chain));

    Utils::throw_if_failed(m_factory->MakeWindowAssociation(
        m_hwnd,
        DXGI_MWA_NO_ALT_ENTER));

    Utils::throw_if_failed(swap_chain.As(&m_swapchain));

    m_frame_index = m_swapchain->GetCurrentBackBufferIndex();
}

void RendererForward::create_rtv_heap()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
    rtv_heap_desc.NumDescriptors = FRAME_COUNT;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    Utils::throw_if_failed(m_device->CreateDescriptorHeap(
        &rtv_heap_desc,
        IID_PPV_ARGS(&m_rtv_heap)));

    m_rtvDescriptorSize =
        m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void RendererForward::create_render_targets()
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle =
        m_rtv_heap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        Utils::throw_if_failed(m_swapchain->GetBuffer(
            i,
            IID_PPV_ARGS(&m_render_targets[i])));

        m_device->CreateRenderTargetView(
            m_render_targets[i].Get(),
            nullptr,
            rtv_handle);

        rtv_handle.ptr += m_rtvDescriptorSize;
    }
}

void RendererForward::create_root_signature()
{
    D3D12_ROOT_SIGNATURE_DESC root_sig_desc{};
    root_sig_desc.NumParameters = 0;
    root_sig_desc.pParameters = nullptr;
    root_sig_desc.NumStaticSamplers = 0;
    root_sig_desc.pStaticSamplers = nullptr;
    root_sig_desc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;

    Utils::throw_if_failed(D3D12SerializeRootSignature(
        &root_sig_desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &error));

    Utils::throw_if_failed(m_device->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&m_root_signature)));
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

    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    {
        blend_desc.RenderTarget[i] = default_render_target_blend_desc;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.InputLayout = { input_layout, _countof(input_layout) };
    pso_desc.pRootSignature = m_root_signature.Get();
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
    pso_desc.DepthStencilState.DepthEnable = FALSE;
    pso_desc.DepthStencilState.StencilEnable = FALSE;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.SampleDesc.Count = 1;

    Utils::throw_if_failed(m_device->CreateGraphicsPipelineState(
        &pso_desc,
        IID_PPV_ARGS(&m_pipeline_state)));
}

void RendererForward::create_meshbuffers()
{
    MeshGeometry triangle_vertices = MeshGeometry::generate_triangle();

    const UINT vertex_buffer_size = triangle_vertices.get_buffer_size();

    D3D12_HEAP_PROPERTIES heap_props{};
    heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1;
    heap_props.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Alignment = 0;
    resource_desc.Width = vertex_buffer_size;
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    Utils::throw_if_failed(m_device->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertex_buffer)));

    void* mapped_data = nullptr;

    D3D12_RANGE read_range{};
    read_range.Begin = 0;
    read_range.End = 0;

    Utils::throw_if_failed(m_vertex_buffer->Map(0, &read_range, &mapped_data));
    memcpy(mapped_data, triangle_vertices.vertices.data(), vertex_buffer_size);
    m_vertex_buffer->Unmap(0, nullptr);

    m_vertex_buffer_view.BufferLocation = m_vertex_buffer->GetGPUVirtualAddress();
    m_vertex_buffer_view.StrideInBytes = triangle_vertices.get_element_size();
    m_vertex_buffer_view.SizeInBytes = vertex_buffer_size;
}

void RendererForward::renderer_render()
{
    Utils::throw_if_failed(m_command_allocator[m_frame_index]->Reset());

    Utils::throw_if_failed(m_command_list->Reset(
        m_command_allocator[m_frame_index].Get(),
        m_pipeline_state.Get()));

    D3D12_RESOURCE_BARRIER barrier_to_rt{};
    barrier_to_rt.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_to_rt.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_to_rt.Transition.pResource = m_render_targets[m_frame_index].Get();
    barrier_to_rt.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier_to_rt.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier_to_rt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_command_list->ResourceBarrier(1, &barrier_to_rt);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle =
        m_rtv_heap->GetCPUDescriptorHandleForHeapStart();
    rtv_handle.ptr += static_cast<SIZE_T>(m_frame_index) * m_rtvDescriptorSize;

    m_command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissor_rect{};
    scissor_rect.left = 0;
    scissor_rect.top = 0;
    scissor_rect.right = static_cast<LONG>(m_width);
    scissor_rect.bottom = static_cast<LONG>(m_height);

    m_command_list->RSSetViewports(1, &viewport);
    m_command_list->RSSetScissorRects(1, &scissor_rect);

    const float clear_color[] = { 0.1f, 0.1f, 0.15f, 1.0f };
    m_command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

    m_command_list->SetGraphicsRootSignature(m_root_signature.Get());

    m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_command_list->IASetVertexBuffers(0, 1, &m_vertex_buffer_view);

    m_command_list->DrawInstanced(3, 1, 0, 0);

    D3D12_RESOURCE_BARRIER barrier_to_present{};
    barrier_to_present.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_to_present.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_to_present.Transition.pResource = m_render_targets[m_frame_index].Get();
    barrier_to_present.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier_to_present.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier_to_present.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_command_list->ResourceBarrier(1, &barrier_to_present);

    Utils::throw_if_failed(m_command_list->Close());

    ID3D12CommandList* command_lists[] =
    {
        m_command_list.Get()
    };

    m_command_queue->ExecuteCommandLists(
        _countof(command_lists),
        command_lists);

    Utils::throw_if_failed(m_swapchain->Present(1, 0));

    move_to_next_frame();
}

void RendererForward::move_to_next_frame()
{
    const UINT64 current_fence_value = m_fence_values[m_frame_index];

    Utils::throw_if_failed(m_command_queue->Signal(
        m_fence.Get(),
        current_fence_value));

    m_frame_index = m_swapchain->GetCurrentBackBufferIndex();

    if (m_fence->GetCompletedValue() < m_fence_values[m_frame_index])
    {
        Utils::throw_if_failed(m_fence->SetEventOnCompletion(
            m_fence_values[m_frame_index],
            m_fence_event));

        WaitForSingleObject(m_fence_event, INFINITE);
    }

    m_fence_values[m_frame_index] = current_fence_value + 1;
}

void RendererForward::wait_for_gpu()
{
    Utils::throw_if_failed(m_command_queue->Signal(
        m_fence.Get(),
        m_fence_values[m_frame_index]));

    Utils::throw_if_failed(m_fence->SetEventOnCompletion(
        m_fence_values[m_frame_index],
        m_fence_event));

    WaitForSingleObjectEx(m_fence_event, INFINITE, FALSE);

    m_fence_values[m_frame_index]++;
}