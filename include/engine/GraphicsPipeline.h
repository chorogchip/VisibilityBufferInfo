#pragma once

#include <array>

#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl.h>

namespace eng {

    class GraphicsPipeline {
    public:
        enum class EnumShader { VS, PS, CS };

        void init(ID3D12Device* device);
        void set_root_signature(ID3D12RootSignature* root_signature);
        void set_shader_vertex(ID3DBlob* shader);
        void set_shader_pixel(ID3DBlob* shader);
        void set_shader_compute(ID3DBlob* shader);
        void set_depth_only();
        void set_depth_equal();
        void set_render_targets(UINT count, DXGI_FORMAT format);
        void set_render_targets(UINT count, const DXGI_FORMAT* formats);
        void set_fullscreen();
        void build();

        [[nodiscard]] ID3D12PipelineState* get() const { return pso_.Get(); }
        [[nodiscard]] ID3D12RootSignature* get_root_signature() const { return root_signature_.Get(); }

    private:
        ID3D12Device* device_ = nullptr;
        Microsoft::WRL::ComPtr<ID3DBlob> vertex_shader_;
        Microsoft::WRL::ComPtr<ID3DBlob> pixel_shader_;
        Microsoft::WRL::ComPtr<ID3DBlob> compute_shader_;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
        bool depth_only_ = false;
        bool depth_equal_ = false;
        bool fullscreen_ = false;
        UINT render_target_count_ = 1;
        std::array<DXGI_FORMAT, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> render_target_formats_{};
    };

}
