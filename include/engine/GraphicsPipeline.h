#pragma once

#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl.h>

namespace eng {

    class GraphicsPipeline {
    public:
        void init(ID3D12Device* device);
        void set_texture_count(UINT texture_count);
        void set_shaders(ID3DBlob* vertex_shader, ID3DBlob* pixel_shader);
        void set_depth_only();
        void set_depth_equal();
        void set_render_targets(UINT count, DXGI_FORMAT format);
        void set_fullscreen();
        void set_fullscreen_input_count(UINT count);
        void set_bench_scene_resolve();
        void build();

        [[nodiscard]] ID3D12PipelineState* get() const { return pso_.Get(); }
        [[nodiscard]] ID3D12RootSignature* get_root_signature() const { return root_signature_.Get(); }

    private:
        void build_root_signature();

        ID3D12Device* device_ = nullptr;
        UINT texture_count_ = 0;
        Microsoft::WRL::ComPtr<ID3DBlob> vertex_shader_;
        Microsoft::WRL::ComPtr<ID3DBlob> pixel_shader_;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
        bool depth_only_ = false;
        bool depth_equal_ = false;
        bool fullscreen_ = false;
        UINT render_target_count_ = 1;
        DXGI_FORMAT render_target_format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
        UINT fullscreen_input_count_ = 1;
        bool bench_scene_resolve_ = false;
    };

}
