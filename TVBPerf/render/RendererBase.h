#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <array>
#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include "dx_util/DX12Context.h"
#include "dx_util/DX12FrameContext.h"
#include "dx_util/DX12List.h"
#include "dx_util/DX12Fence.h"
#include "dx_util/DX12QueryBuf.h"

#include "scene/SceneDataGPU.h"

#include "render/ProcedureBase.h"

class RendererBase : public rndr::ProcedureBase
{
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    static constexpr UINT FRAME_COUNT = 2;

    RendererBase() = default;
    virtual ~RendererBase() = default;
    void init(const util::ProgramArgument& args, HWND hwnd);
    void render();
    void close();

protected:

    virtual void init_() = 0;
    virtual void render_() = 0;

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

    void create_meshbuffers();
    void create_dummy_textures();
    void create_constbuffers();
    void create_depth_stencil_resources();
    void create_render_target_views();
    void create_shader_visible_srv_heap();

    void move_to_next_frame();

protected:
    
    dxutl::DX12Context dx12_context_;
    std::array<dxutl::DX12FrameContext, FRAME_COUNT> dx12_frame_contexts_;
    dxutl::DX12List dx12_list_;

    dxutl::DX12Fence dx12_fence_;
    dxutl::DX12QueryBuf dx12_query_buf_;

    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissor_rect_{};

    dxutl::DX12FrameContext& curr_context();



    constexpr static DXGI_FORMAT DEPTH_STENCIL_FORMAT_ = DXGI_FORMAT_D32_FLOAT;
    ComPtr<ID3D12DescriptorHeap> dsv_heap_;
    ComPtr<ID3D12Resource> depth_stencil_buffer_;
    UINT dsv_descriptor_size_ = 0;

    ComPtr<ID3D12DescriptorHeap> rtv_heap_;
    UINT rtv_descriptor_size_ = 0;
    ComPtr<ID3D12Resource> render_targets_[FRAME_COUNT];

    ComPtr<ID3D12DescriptorHeap> srv_heap_;
    UINT srv_descriptor_size_ = 0;

    ComPtr<ID3D12RootSignature> root_signature_;
    ComPtr<ID3D12PipelineState> pipeline_state_;

    std::unique_ptr<scene::SceneDataGPU> scene_gpu_;

    struct ConstBufMatrices {
        DirectX::XMFLOAT4X4 mat_view_;
        DirectX::XMFLOAT4X4 mat_proj_;
        DirectX::XMFLOAT2 viewport_size_;
        DirectX::XMFLOAT2 inv_viewport_size_;
    } matrix_buf_cpu_{};
    ComPtr<ID3D12Resource> buf_constant_[FRAME_COUNT];
    void* buf_constant_mapped_[FRAME_COUNT]{};

    std::vector<ComPtr<ID3D12Resource>> dummy_textures_;

    static constexpr float CLEAR_COLOR_[] = { 0.1f, 0.1f, 0.15f, 1.0f };
    
public:
};
