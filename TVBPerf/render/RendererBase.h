#pragma once

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <cstdint>
#include <memory>

#include "util/ProgramArgument.h"
#include "util/GPUFrameTime.h"
#include "scene/SceneSynthSphere.h"
#include "scene/SceneSynthSphereRuntime.h"

class RendererBase
{
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    static constexpr UINT FRAME_COUNT = 2;

    ~RendererBase();
    void init(HWND hwnd, const ProgramArgument&);
    void render();
    bool to_terminate() const { return to_terminate_; }

private:
    void create_dsv_heap();
    void create_rtv_heap();
    void create_render_targets();
    void create_depth_stencil_buffer();

    void create_root_signature();
    void create_pso();
    void create_meshbuffers(const ProgramArgument& arg);
    void create_constbuffers(const ProgramArgument& arg);
    void create_instancebuffers();

    void create_timestamp_queries();

    void wait_for_gpu();
    void move_to_next_frame();
    void read_gpu_timestamp_for_frame(UINT frame_index);

private:
    HWND hwnd_ = nullptr;
    uint32_t width_ = 0;
    uint32_t height_ = 0;

    bool to_terminate_;
    size_t frame_count_;
    size_t frame_warmup_count_;
    size_t frame_measure_count_;

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
    } matrix_buf_cpu_{};
    ComPtr<ID3D12Resource> buf_constant_;

    ComPtr<ID3D12Resource> buf_instance_;

    GpuFrameTime<FRAME_COUNT> frame_time;


};