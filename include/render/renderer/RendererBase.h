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
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"

#include "scene/SceneDataCPU.h"
#include "scene/SceneDataGPU.h"

#include "render/Camera.h"
#include "render/CameraPathController.h"

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
    virtual void render_() = 0;

    virtual void make_programresult(util::ProgramResult& result) = 0;
    virtual void create_renderer_resources();

    virtual D3D12_RESOURCE_STATES depth_stencil_initial_state() const;
    virtual void init_passes() = 0;

    void copy_camera_data();
    void present();

private:
    void create_command_objects();

    void init_viewport_scissor_rect();
    void load_scene();
    void create_scene_resources();
    void create_frame_resources();
    void create_back_buffer_resources();
    void create_benchmark_resources();
    void create_sampler_descriptors();
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
    ComPtr<ID3D12Resource> depth_stencil_buffer_;
    ComPtr<ID3D12Resource> render_targets_[util::FRAME_COUNT];

    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissor_rect_{};

    UINT64 fence_values_[util::FRAME_COUNT]{};

    std::unique_ptr<scene::SceneDataCPU> scene_cpu_;
    std::unique_ptr<scene::SceneDataGPU> scene_gpu_;

    struct ConstBufMatrices {
        DirectX::XMFLOAT4X4 mat_view_;
        DirectX::XMFLOAT4X4 mat_proj_;
        DirectX::XMFLOAT2 viewport_size_;
    };
    ConstBufMatrices matrix_buf_cpu_{};
    dxutl::UploadConstBuf<ConstBufMatrices> buf_constant_[util::FRAME_COUNT];

    std::vector<ComPtr<ID3D12Resource>> dummy_textures_;

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
