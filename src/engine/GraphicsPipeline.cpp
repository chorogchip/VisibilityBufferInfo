#include "engine/GraphicsPipeline.h"

#include "engine/RootSignatureBuilder.h"
#include "util/Logger.h"

namespace eng {

    namespace {
        D3D12_INPUT_LAYOUT_DESC default_input_layout() {
            static constexpr D3D12_INPUT_ELEMENT_DESC elements[] = {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 32,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 40,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };
            return { elements, _countof(elements) };
        }
    }

    void GraphicsPipeline::init(ID3D12Device* device) {
        device_ = device;
        texture_count_ = 0;
        vertex_shader_.Reset();
        pixel_shader_.Reset();
        root_signature_.Reset();
        pso_.Reset();
        depth_only_ = false;
        depth_equal_ = false;
        fullscreen_ = false;
        render_target_count_ = 1;
        render_target_format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
        fullscreen_input_count_ = 1;
        bench_scene_resolve_ = false;
    }

    void GraphicsPipeline::set_texture_count(UINT texture_count) {
        texture_count_ = texture_count;
    }

    void GraphicsPipeline::set_shaders(ID3DBlob* vertex_shader, ID3DBlob* pixel_shader) {
        vertex_shader_ = vertex_shader;
        pixel_shader_ = pixel_shader;
    }

    void GraphicsPipeline::set_depth_only() {
        depth_only_ = true;
    }

    void GraphicsPipeline::set_depth_equal() {
        depth_equal_ = true;
    }

    void GraphicsPipeline::set_render_targets(UINT count, DXGI_FORMAT format) {
        render_target_count_ = count;
        render_target_format_ = format;
    }

    void GraphicsPipeline::set_fullscreen() {
        fullscreen_ = true;
    }

    void GraphicsPipeline::set_fullscreen_input_count(UINT count) {
        fullscreen_input_count_ = count;
    }

    void GraphicsPipeline::set_bench_scene_resolve() {
        bench_scene_resolve_ = true;
    }

    void GraphicsPipeline::build() {
        build_root_signature();

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.InputLayout = fullscreen_ ? D3D12_INPUT_LAYOUT_DESC{} : default_input_layout();
        desc.pRootSignature = root_signature_.Get();
        desc.VS = { vertex_shader_->GetBufferPointer(), vertex_shader_->GetBufferSize() };
        if (pixel_shader_)
            desc.PS = { pixel_shader_->GetBufferPointer(), pixel_shader_->GetBufferSize() };

        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        desc.RasterizerState.FrontCounterClockwise = FALSE;
        desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        desc.RasterizerState.DepthClipEnable = TRUE;
        desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        const D3D12_RENDER_TARGET_BLEND_DESC render_target_blend{
            FALSE,
            FALSE,
            D3D12_BLEND_ONE,
            D3D12_BLEND_ZERO,
            D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE,
            D3D12_BLEND_ZERO,
            D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL
        };
        for (auto& target : desc.BlendState.RenderTarget)
            target = render_target_blend;

        desc.DepthStencilState.DepthEnable = !fullscreen_;
        desc.DepthStencilState.DepthWriteMask = depth_equal_
            ? D3D12_DEPTH_WRITE_MASK_ZERO
            : D3D12_DEPTH_WRITE_MASK_ALL;
        desc.DepthStencilState.DepthFunc = depth_equal_
            ? D3D12_COMPARISON_FUNC_EQUAL
            : D3D12_COMPARISON_FUNC_LESS;
        desc.DepthStencilState.StencilEnable = FALSE;
        desc.SampleMask = UINT_MAX;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = depth_only_ ? 0 : render_target_count_;
        for (UINT i = 0; i < desc.NumRenderTargets; ++i)
            desc.RTVFormats[i] = render_target_format_;
        desc.DSVFormat = fullscreen_ ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;

        const HRESULT result = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso_));
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "create graphics pipeline state");
    }

    void GraphicsPipeline::build_root_signature() {
        RootSignatureBuilder builder;

        if (fullscreen_) {
            if (bench_scene_resolve_) {
                builder
                    .add_root_cbv(0, 0, D3D12_SHADER_VISIBILITY_PIXEL)
                    .srv_table().base(0).cnt(1).spc(1).vis(D3D12_SHADER_VISIBILITY_PIXEL).add()
                    .sampler_table().base(0).cnt(1).spc(1).vis(D3D12_SHADER_VISIBILITY_PIXEL).add()
                    .srv_table().base(0).cnt(6).vis(D3D12_SHADER_VISIBILITY_PIXEL).add()
                    .srv_table().base(8).cnt(texture_count_).vis(D3D12_SHADER_VISIBILITY_PIXEL).add()
                    .sampler_table().base(0).cnt(1).vis(D3D12_SHADER_VISIBILITY_PIXEL).add();
            }
            else {
                builder
                    .add_root_cbv(0)
                    .srv_table().base(0).cnt(fullscreen_input_count_).add()
                    .sampler_table().base(0).cnt(1).add();
            }
        }
        else {
            const UINT texture_descriptor_count = texture_count_ > 0 ? texture_count_ : 1;
            builder
                .set_flags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
                .add_root_cbv(0, 0, D3D12_SHADER_VISIBILITY_VERTEX)
                .add_constants(1, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX)
                .add_root_srv(0, 1, D3D12_SHADER_VISIBILITY_VERTEX)
                .add_root_srv(1, 1, D3D12_SHADER_VISIBILITY_PIXEL)
                .srv_table().base(2).cnt(3).spc(1).add()
                .srv_table().base(0).cnt(texture_descriptor_count).spc(2).vis(D3D12_SHADER_VISIBILITY_PIXEL).add()
                .sampler_table().base(0).cnt(1).spc(2).vis(D3D12_SHADER_VISIBILITY_PIXEL).add()
                .add_root_srv(0, 0, D3D12_SHADER_VISIBILITY_VERTEX)
                .add_root_srv(1, 0, D3D12_SHADER_VISIBILITY_PIXEL)
                .srv_table().base(8).cnt(texture_descriptor_count).vis(D3D12_SHADER_VISIBILITY_PIXEL).add()
                .sampler_table().base(0).cnt(1).vis(D3D12_SHADER_VISIBILITY_PIXEL).add();
        }

        root_signature_ = builder.build(device_);
    }

}
