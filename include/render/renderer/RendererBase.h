#pragma once

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "ProgramArgument.h"
#include "util/Constants.h"
#include "util/Logger.h"
#include "util/FrameCounter.h"
#include "dx_util/GPUFrameTimer.h"
#include "dx_util/UploadConstBuf.h"
#include "engine/GPUQueue.h"
#include "engine/GPUResource.h"
#include "render/camera/Camera.h"
#include "render/camera/CameraPathController.h"

class RendererBase
{
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    virtual ~RendererBase();
    void init(HWND hwnd, const util::ProgramArgument& arg);
    void close();
    void render();
    bool to_terminate() const { return frame_counter_.to_terminate(); }

    rndr::Camera camera_{};

protected:
    virtual void init1_() = 0;
    virtual void render_prepare_() = 0;
    virtual void render_record_() = 0;
    virtual void before_close_() {}

    util::ProgramArgument program_argument_{};
    util::ProgramResult program_result_{};
    bool to_profile_index_count_ = false;
    double profile_index_count_ = 0.0;

    HWND hwnd_ = nullptr;
    uint32_t width_ = 0;
    uint32_t height_ = 0;

    ComPtr<IDXGIFactory4> factory_;
    ComPtr<ID3D12Device> device_;

    eng::GPUQueue graphics_queue_;
    UINT64 fence_values_[util::Constants::FRAME_COUNT]{};
    ComPtr<ID3D12CommandAllocator> command_allocator_[util::Constants::FRAME_COUNT];
    ComPtr<ID3D12GraphicsCommandList> command_list_;

    ComPtr<IDXGISwapChain3> swapchain_;
    UINT frame_index_ = 0;

    eng::GPUResource render_targets_[util::Constants::FRAME_COUNT];

    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissor_rect_{};

    rndr::CameraPathController camera_path_controller_{};
    util::FrameCounter frame_counter_{};
    dxutl::GpuFrameTimer frame_time_{};
};
