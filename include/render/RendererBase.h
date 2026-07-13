#pragma once

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "util/Logger.h"
#include "util/ProgramArgument.h"
#include "util/FrameCounter.h"
#include "dx_util/GPUFrameTimer.h"
#include "dx_util/Fence.h"

#include "scene/SceneDataCPU.h"
#include "scene/SceneDataGPU.h"

#include "render/Camera.h"

class RendererBase
{
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    static constexpr UINT FRAME_COUNT = 2;

    virtual ~RendererBase();
    void init(HWND hwnd, const util::ProgramArgument&);
    void render();
    void close();
    bool to_terminate() const { return frame_counter_.to_terminate(); }

protected:
    virtual void render_() = 0;

    virtual void make_programresult(util::ProgramResult& result) = 0;
    virtual void create_pass_resources();

    virtual UINT dsv_descriptor_count() const;
    virtual D3D12_RESOURCE_STATES depth_stencil_initial_state() const;
    virtual void create_extra_depth_stencil_views();

    virtual UINT rtv_descriptor_count() const;
    virtual void create_extra_render_target_views(D3D12_CPU_DESCRIPTOR_HANDLE next_rtv_handle);

    virtual UINT srv_descriptor_count() const;
    virtual void create_shader_resources();

    virtual void create_root_signature() = 0;
    virtual void create_pso() = 0;

    void copy_camera_data();
    void create_texture_srv_descriptors(D3D12_CPU_DESCRIPTOR_HANDLE srv_handle);

private:
    void create_command_objects();

    void init_viewport_scissorrect();
    void create_meshbuffers();
    void create_dummy_textures();
    void create_constbuffers();
    void create_depth_stencil_resources();
    void create_render_target_views();
    void create_shader_visible_srv_heap();
    void create_sampler_heap();
    void create_texture_sampler_descriptors();

    void move_to_next_frame();

protected:
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

    constexpr static DXGI_FORMAT DEPTH_STENCIL_FORMAT_ = DXGI_FORMAT_D32_FLOAT;
    ComPtr<ID3D12DescriptorHeap> dsv_heap_;
    ComPtr<ID3D12Resource> depth_stencil_buffer_;
    UINT dsv_descriptor_size_ = 0;

    ComPtr<ID3D12DescriptorHeap> rtv_heap_;
    UINT rtv_descriptor_size_ = 0;
    ComPtr<ID3D12Resource> render_targets_[FRAME_COUNT];

    ComPtr<ID3D12DescriptorHeap> srv_heap_;
    UINT srv_descriptor_size_ = 0;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> sampler_heap_;
    UINT sampler_descriptor_size_ = 0;

    ComPtr<ID3D12RootSignature> root_signature_;
    ComPtr<ID3D12PipelineState> pipeline_state_;

    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissor_rect_{};

    UINT64 fence_values_[FRAME_COUNT]{};
    dxutl::Fence fence_;

    std::unique_ptr<scene::SceneDataCPU> scene_cpu_;
    std::unique_ptr<scene::SceneDataGPU> scene_gpu_;

    struct ConstBufMatrices {
        DirectX::XMFLOAT4X4 mat_view_;
        DirectX::XMFLOAT4X4 mat_proj_;
        DirectX::XMFLOAT2 viewport_size_;
    } matrix_buf_cpu_{};
    ComPtr<ID3D12Resource> buf_constant_[FRAME_COUNT];
    void* buf_constant_mapped_[FRAME_COUNT]{};

    std::vector<ComPtr<ID3D12Resource>> dummy_textures_;

    static_assert(dxutl::GpuFrameTimer::FRAME_COUNT == FRAME_COUNT, "");
    dxutl::GpuFrameTimer frame_time_;
    util::FrameCounter frame_counter_;

    static constexpr float CLEAR_COLOR_[] = { 0.1f, 0.1f, 0.15f, 1.0f };

    std::unique_ptr<const util::ProgramArgument> program_arguments_;
    
public:
    rndr::Camera camera_{};
};
