#include "dx_util/DxGraphicsPSO.h"

#include "dx_util/DescriptorUtils.h"
#include "render/RootParameter.h"
#include "util/Logger.h"

namespace dxutl {

    void DxGraphicsPSO::init(ID3D12Device* device) {
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

    void DxGraphicsPSO::set_texture_count(UINT texture_count) {
        texture_count_ = texture_count;
    }

    void DxGraphicsPSO::set_shaders(ID3DBlob* vertex_shader, ID3DBlob* pixel_shader) {
        vertex_shader_ = vertex_shader;
        pixel_shader_ = pixel_shader;
    }

    void DxGraphicsPSO::set_depth_only() {
        depth_only_ = true;
    }

    void DxGraphicsPSO::set_depth_equal() {
        depth_equal_ = true;
    }

    void DxGraphicsPSO::set_render_targets(UINT count, DXGI_FORMAT format) {
        render_target_count_ = count;
        render_target_format_ = format;
    }

    void DxGraphicsPSO::set_fullscreen() {
        fullscreen_ = true;
    }

    void DxGraphicsPSO::set_fullscreen_input_count(UINT count) {
        fullscreen_input_count_ = count;
    }

    void DxGraphicsPSO::set_bench_scene_resolve() {
        bench_scene_resolve_ = true;
    }

    void DxGraphicsPSO::build() {
        HRESULT result = build_root_signature();
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "create graphics root signature");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.InputLayout = fullscreen_ ? D3D12_INPUT_LAYOUT_DESC{} : get_default_input_layout_desc();
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

        result = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso_));
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "create graphics pipeline state");
    }

    HRESULT DxGraphicsPSO::build_root_signature() {
        if (fullscreen_) {
            if (bench_scene_resolve_) {
                D3D12_DESCRIPTOR_RANGE ranges[4]{};
                ranges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1,
                    D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
                ranges[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 1,
                    D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
                ranges[2] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0, 0,
                    D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
                ranges[3] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, texture_count_, 8, 0,
                    D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
                D3D12_DESCRIPTOR_RANGE bench_sampler{
                    D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0,
                    D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };

                D3D12_ROOT_PARAMETER parameters[
                    rndr::root_param(rndr::EnumRootParamFullscreen::COUNT)]{};
                auto set_table = [&](rndr::EnumRootParamFullscreen index,
                                     D3D12_DESCRIPTOR_RANGE& range) {
                    auto& parameter = parameters[rndr::root_param(index)];
                    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    parameter.DescriptorTable = { 1, &range };
                    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
                };
                auto& constants = parameters[rndr::root_param(
                    rndr::EnumRootParamFullscreen::PASS_CONSTANT)];
                constants.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                constants.Descriptor.ShaderRegister = 0;
                constants.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
                set_table(rndr::EnumRootParamFullscreen::INPUT_SRV, ranges[0]);
                set_table(rndr::EnumRootParamFullscreen::SAMPLER, ranges[1]);
                set_table(rndr::EnumRootParamFullscreen::BENCH_INPUT_SRV, ranges[2]);
                set_table(rndr::EnumRootParamFullscreen::BENCH_MATERIAL_TEXTURE, ranges[3]);
                set_table(rndr::EnumRootParamFullscreen::BENCH_MATERIAL_SAMPLER, bench_sampler);

                D3D12_ROOT_SIGNATURE_DESC desc{};
                desc.NumParameters = _countof(parameters);
                desc.pParameters = parameters;
                Microsoft::WRL::ComPtr<ID3DBlob> signature;
                Microsoft::WRL::ComPtr<ID3DBlob> error;
                HRESULT result = D3D12SerializeRootSignature(
                    &desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
                if (FAILED(result)) return result;
                return device_->CreateRootSignature(0, signature->GetBufferPointer(),
                    signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_));
            }

            D3D12_DESCRIPTOR_RANGE ranges[2]{};
            ranges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, fullscreen_input_count_, 0, 0,
                D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
            ranges[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0,
                D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };

            D3D12_ROOT_PARAMETER parameters[3]{};
            auto& constants = parameters[rndr::root_param(rndr::EnumRootParamFullscreen::PASS_CONSTANT)];
            constants.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            constants.Descriptor.ShaderRegister = 0;
            auto& inputs = parameters[rndr::root_param(rndr::EnumRootParamFullscreen::INPUT_SRV)];
            inputs.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            inputs.DescriptorTable = { 1, &ranges[0] };
            auto& sampler = parameters[rndr::root_param(rndr::EnumRootParamFullscreen::SAMPLER)];
            sampler.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            sampler.DescriptorTable = { 1, &ranges[1] };

            D3D12_ROOT_SIGNATURE_DESC desc{};
            desc.NumParameters = _countof(parameters);
            desc.pParameters = parameters;
            Microsoft::WRL::ComPtr<ID3DBlob> signature;
            Microsoft::WRL::ComPtr<ID3DBlob> error;
            HRESULT result = D3D12SerializeRootSignature(
                &desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
            if (FAILED(result))
                return result;
            return device_->CreateRootSignature(0, signature->GetBufferPointer(),
                signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_));
        }

        D3D12_DESCRIPTOR_RANGE ranges[5]{};
        ranges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 2, 1,
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        ranges[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, texture_count_, 0, 2,
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        ranges[2] = { D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 2,
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        ranges[3] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, texture_count_, 8, 0,
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        ranges[4] = { D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0,
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };

        D3D12_ROOT_PARAMETER parameters[
            rndr::root_param(rndr::EnumRootParamScene::COUNT)]{};

        auto& frame = parameters[rndr::root_param(rndr::EnumRootParamScene::FRAME_CONSTANT)];
        frame.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        frame.Descriptor.ShaderRegister = 0;
        frame.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        auto& draw = parameters[rndr::root_param(rndr::EnumRootParamScene::DRAW_CONSTANT)];
        draw.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        draw.Constants.ShaderRegister = 1;
        draw.Constants.Num32BitValues = 1;
        draw.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        auto set_srv = [&](rndr::EnumRootParamScene index, UINT shader_register,
                           UINT space, D3D12_SHADER_VISIBILITY visibility) {
            auto& parameter = parameters[rndr::root_param(index)];
            parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
            parameter.Descriptor.ShaderRegister = shader_register;
            parameter.Descriptor.RegisterSpace = space;
            parameter.ShaderVisibility = visibility;
        };
        set_srv(rndr::EnumRootParamScene::INSTANCE_BUFFER, 0, 1, D3D12_SHADER_VISIBILITY_VERTEX);
        set_srv(rndr::EnumRootParamScene::MATERIAL_BUFFER, 1, 1, D3D12_SHADER_VISIBILITY_PIXEL);
        set_srv(rndr::EnumRootParamScene::BENCH_INSTANCE_BUFFER, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
        set_srv(rndr::EnumRootParamScene::BENCH_MATERIAL_BUFFER, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);

        auto set_table = [&](rndr::EnumRootParamScene index, D3D12_DESCRIPTOR_RANGE& range,
                             D3D12_SHADER_VISIBILITY visibility) {
            auto& parameter = parameters[rndr::root_param(index)];
            parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameter.DescriptorTable.NumDescriptorRanges = 1;
            parameter.DescriptorTable.pDescriptorRanges = &range;
            parameter.ShaderVisibility = visibility;
        };
        set_table(rndr::EnumRootParamScene::GEOMETRY_BUFFER, ranges[0], D3D12_SHADER_VISIBILITY_ALL);
        set_table(rndr::EnumRootParamScene::MATERIAL_TEXTURE, ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
        set_table(rndr::EnumRootParamScene::MATERIAL_SAMPLER, ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
        set_table(rndr::EnumRootParamScene::BENCH_MATERIAL_TEXTURE, ranges[3], D3D12_SHADER_VISIBILITY_PIXEL);
        set_table(rndr::EnumRootParamScene::BENCH_MATERIAL_SAMPLER, ranges[4], D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_ROOT_SIGNATURE_DESC root_desc{};
        root_desc.NumParameters = _countof(parameters);
        root_desc.pParameters = parameters;
        root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;
        HRESULT result = D3D12SerializeRootSignature(
            &root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        if (FAILED(result))
            return result;

        return device_->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&root_signature_));
    }

}
