#include "engine/GraphicsPipeline.h"

#include <algorithm>

#include "util/Utils.h"
#include "util/Logger.h"

namespace eng {

    static D3D12_INPUT_LAYOUT_DESC default_input_layout() {
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

    void GraphicsPipeline::init(ID3D12Device* device) {
        device_ = device;
        pipeline_type_ = PipelineType::Undefined;
        vertex_shader_.Reset();
        pixel_shader_.Reset();
        compute_shader_.Reset();
        root_signature_.Reset();
        pso_.Reset();
        depth_only_ = false;
        depth_equal_ = false;
        fullscreen_ = false;
        manual_vertex_fetch_ = false;
        render_target_count_ = 1;
        render_target_formats_.fill(DXGI_FORMAT_UNKNOWN);
        render_target_formats_[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    void GraphicsPipeline::set_graphics() {
        pipeline_type_ = PipelineType::Graphics;
    }

    void GraphicsPipeline::set_compute() {
        pipeline_type_ = PipelineType::Compute;
    }

    void GraphicsPipeline::set_root_signature(ID3D12RootSignature* root_signature) {
        root_signature_ = root_signature;
    }


    void GraphicsPipeline::set_shader_vertex(ID3DBlob* shader) {
        vertex_shader_ = shader;
    }
    void GraphicsPipeline::set_shader_pixel(ID3DBlob* shader) {
        pixel_shader_ = shader;
    }
    void GraphicsPipeline::set_shader_compute(ID3DBlob* shader) {
        compute_shader_ = shader;
    }

    void GraphicsPipeline::set_manual_vertex_fetch() {
        manual_vertex_fetch_ = true;
    }

    void GraphicsPipeline::set_depth_only() {
        depth_only_ = true;
    }

    void GraphicsPipeline::set_depth_equal() {
        depth_equal_ = true;
    }

    void GraphicsPipeline::set_render_targets(UINT count, DXGI_FORMAT format) {
        util::Logger::g_logger.assert_with_log(
            count <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT,
            "invalid render target format request");

        render_target_count_ = count;
        render_target_formats_.fill(DXGI_FORMAT_UNKNOWN);
        for (UINT i = 0; i < count; ++i)
            render_target_formats_[i] = format;
    }

    void GraphicsPipeline::set_render_targets(UINT count, const DXGI_FORMAT* formats) {
        util::Logger::g_logger.assert_with_log(
            formats != nullptr &&
            count <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT,
            "invalid render target format request");

        render_target_count_ = count;
        render_target_formats_.fill(DXGI_FORMAT_UNKNOWN);
        std::copy_n(formats, count, render_target_formats_.begin());
    }

    void GraphicsPipeline::set_fullscreen() {
        fullscreen_ = true;
    }

    void GraphicsPipeline::build() {
        util::Logger::g_logger.assert_with_log(
            device_ != nullptr && root_signature_ != nullptr,
            "pipeline requires a device and root signature");

        if (pipeline_type_ == PipelineType::Compute) {
            util::Logger::g_logger.assert_with_log(
                compute_shader_ != nullptr,
                "compute pipeline requires a compute shader");

            D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
            desc.pRootSignature = root_signature_.Get();
            desc.CS = {
                compute_shader_->GetBufferPointer(),
                compute_shader_->GetBufferSize()
            };

            const HRESULT result = device_->CreateComputePipelineState(
                &desc, IID_PPV_ARGS(&pso_));
            util::Logger::g_logger.assert_with_log(
                SUCCEEDED(result), "create compute pipeline state");
            return;
        }

        util::Logger::g_logger.assert_with_log(
            pipeline_type_ == PipelineType::Graphics,
            "pipeline type must be set before build");
        util::Logger::g_logger.assert_with_log(
            vertex_shader_ != nullptr,
            "graphics pipeline requires a vertex shader");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.InputLayout = fullscreen_ || manual_vertex_fetch_
            ? D3D12_INPUT_LAYOUT_DESC{}
            : default_input_layout();
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
            desc.RTVFormats[i] = render_target_formats_[i];
        desc.DSVFormat = fullscreen_ ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;

        util::Utils::throw_if_failed(
            device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso_)),
            "create graphics pipeline state");
    }

}
