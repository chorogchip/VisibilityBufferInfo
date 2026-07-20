#pragma once

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "Constants.h"
#include "ProgramArgument.h"
#include "util/Logger.h"
#include "util/FrameCounter.h"
#include "dx_util/GPUFrameTimer.h"
#include "dx_util/UploadConstBuf.h"
#include "engine/GraphicsQueue.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"

#include "render/camera/Camera.h"
#include "render/camera/CameraPathController.h"

class RendererBase
{
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    virtual ~RendererBase();
    void init(HWND hwnd, const util::ProgramArgument&);
    void render();
    void close();
    bool to_terminate() const { return frame_counter_.to_terminate(); }

protected:
    virtual void init_programresult_(util::ProgramResult& result) = 0;
    virtual void init_scene_() {}
    virtual void init_shader_resources_() = 0;
    virtual void init_renderer_resources_();
    virtual void init_passes_() = 0;
    virtual void record_render_commands_() = 0;

private:
    void init_runtime(HWND hwnd, const util::ProgramArgument& arg);
    void init_gpu(const util::ProgramArgument& arg);
    void init_renderer();

    void copy_camera_data();
    void begin_frame();
    void submit_frame();
    void present();
    void create_command_objects();

    void init_viewport_scissor_rect();
    void create_frame_resources();
    void create_back_buffer_resources();
    void transition_back_buffers_();
    void move_to_next_frame();

protected:
    HWND hwnd_ = nullptr;
    uint32_t width_ = 0;
    uint32_t height_ = 0;

    ComPtr<IDXGIFactory4> factory_;
    ComPtr<ID3D12Device> device_;

    eng::GraphicsQueue graphics_queue_;
    ComPtr<ID3D12CommandAllocator> command_allocator_[util::FRAME_COUNT];
    ComPtr<ID3D12GraphicsCommandList> command_list_;

    ComPtr<IDXGISwapChain3> swapchain_;
    UINT frame_index_ = 0;

    constexpr static DXGI_FORMAT DEPTH_STENCIL_FORMAT_ = DXGI_FORMAT_D32_FLOAT;
    eng::GPUResource depth_stencil_buffer_;
    eng::GPUResource render_targets_[util::FRAME_COUNT];

    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissor_rect_{};

    UINT64 fence_values_[util::FRAME_COUNT]{};

    struct ConstBufMatrices {
        DirectX::XMFLOAT4X4 mat_view_;
        DirectX::XMFLOAT4X4 mat_proj_;
        DirectX::XMFLOAT2 viewport_size_;
    };
    ConstBufMatrices matrix_buf_cpu_{};
    dxutl::UploadConstBuf<ConstBufMatrices> buf_constant_[util::FRAME_COUNT];

    dxutl::GpuFrameTimer frame_time_;
    util::FrameCounter frame_counter_;
    rndr::CameraPathController camera_path_controller_;

    static constexpr float CLEAR_COLOR_[] = { 0.1f, 0.1f, 0.15f, 1.0f };

    std::unique_ptr<const util::ProgramArgument> program_arguments_;

protected:
    eng::ResourceManagerFrame resource_manager_frame_;
    eng::ResourceManagerSampler resource_manager_sampler_;
    eng::ResourceManagerShader resource_manager_shader_;
    
public:
    rndr::Camera camera_{};
};
