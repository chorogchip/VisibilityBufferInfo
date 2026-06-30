#pragma once

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <cstdint>

class RendererForward
{
private:
    static constexpr UINT FRAME_COUNT = 2;

public:
    void renderer_init(HWND hwnd, uint32_t width, uint32_t height);
    void renderer_render();

private:
    void create_device();
    void create_command_objects();
    void create_swapchain();
    void create_rtv_heap();
    void create_render_targets();

    void create_root_signature();
    void create_pso();
    void create_meshbuffers();

    void wait_for_gpu();
    void move_to_next_frame();

private:
    HWND m_hwnd = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    Microsoft::WRL::ComPtr<IDXGIFactory4> m_factory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_command_queue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_command_allocator[FRAME_COUNT];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_command_list;

    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapchain;
    UINT m_frame_index = 0;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtv_heap;
    UINT m_rtvDescriptorSize = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_render_targets[FRAME_COUNT];

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_root_signature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipeline_state;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertex_buffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertex_buffer_view{};

    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fence_values[FRAME_COUNT]{};
    HANDLE m_fence_event = nullptr;
};