#pragma once

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <cstdint>
#include <memory>

#include "SceneSynthSphere.h"
#include "SceneSynthSphereRuntime.h"

class RendererForward
{
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

private:
    static constexpr UINT FRAME_COUNT = 2;

public:
    void init(HWND hwnd, uint32_t width, uint32_t height);
    void render();

private:
    void create_device();
    void create_command_objects();
    void create_swapchain();
    void create_dsv_heap();
    void create_rtv_heap();
    void create_render_targets();
    void create_depth_stencil_buffer();

    void create_root_signature();
    void create_pso();
    void create_meshbuffers();
    void create_constbuffers();
    void create_instancebuffers();

    void wait_for_gpu();
    void move_to_next_frame();

private:
    HWND hwnd_ = nullptr;
    uint32_t width_ = 0;
    uint32_t height_ = 0;

    ComPtr<IDXGIFactory4> factory_;
    ComPtr<ID3D12Device> device_;

    ComPtr<ID3D12CommandQueue> command_queue_;
    ComPtr<ID3D12CommandAllocator> command_allocator_[FRAME_COUNT];
    ComPtr<ID3D12GraphicsCommandList> command_list_;

    ComPtr<IDXGISwapChain3> swapchain_;
    UINT frame_index_ = 0;


    constexpr static DXGI_FORMAT depth_stencil_format_ = DXGI_FORMAT_D32_FLOAT;
    ComPtr<ID3D12DescriptorHeap> dsv_heap_;
    ComPtr<ID3D12Resource> depth_stencil_buffer_;
    UINT dsv_descriptor_size_ = 0;

    ComPtr<ID3D12DescriptorHeap> rtv_heap_;
    UINT rtv_descriptor_size_ = 0;
    ComPtr<ID3D12Resource> render_targets_[FRAME_COUNT];

    ComPtr<ID3D12RootSignature> root_signature_;
    ComPtr<ID3D12PipelineState> pipeline_state_;

    ComPtr<ID3D12Fence> fence_;
    UINT64 fence_values_[FRAME_COUNT]{};
    HANDLE fence_event_ = nullptr;

    std::unique_ptr<SceneSynthSphere> scene_raw_;
    std::unique_ptr<SceneSynthSphereRuntime> scene_resource_;

    struct ConstBufMatrices {
        DirectX::XMFLOAT4X4 mat_view_;
        DirectX::XMFLOAT4X4 mat_proj_;
    } matrix_buf_cpu_;
    ComPtr<ID3D12Resource> buf_constant_;

    ComPtr<ID3D12Resource> buf_instance_;
};