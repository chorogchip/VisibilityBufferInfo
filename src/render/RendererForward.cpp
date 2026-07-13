#include "render/RendererForward.h"

#include <d3d12.h>
#include <string>

#include "util/Utils.h"
#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"

namespace rndr {


    void RendererForward::make_programresult(util::ProgramResult& result) {
        result.renderer_name = "ForwardPrepass";
        result.pass_name_0 = "depth_prepass";
        result.pass_name_1 = "forward";
        result.pass_name_3 = "total";
    }

    void RendererForward::render_() {

        Utils::throw_if_failed(command_allocator_[frame_index_]->Reset(), "reset command allocator");
        Utils::throw_if_failed(command_list_->Reset(command_allocator_[frame_index_].Get(),
            pipeline_state_.Get()), "command list reset on render start");
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 0);
        this->copy_camera_data();

        dxutl::transition_resource(command_list_.Get(), render_targets_[frame_index_].Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle =
            rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        rtv_handle.ptr += static_cast<SIZE_T>(frame_index_) * rtv_descriptor_size_;
        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
        command_list_->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);

        command_list_->RSSetViewports(1, &viewport_);
        command_list_->RSSetScissorRects(1, &scissor_rect_);

        command_list_->ClearRenderTargetView(rtv_handle, CLEAR_COLOR_, 0, nullptr);
        command_list_->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        command_list_->SetGraphicsRootSignature(root_signature_.Get());
        ID3D12DescriptorHeap* heaps[] = { srv_heap_.Get() };
        command_list_->SetDescriptorHeaps(_countof(heaps), heaps);
        command_list_->SetGraphicsRootConstantBufferView(0, buf_constant_[frame_index_]->GetGPUVirtualAddress());
        command_list_->SetGraphicsRootShaderResourceView(1, scene_gpu_->object_buffer->GetGPUVirtualAddress());
        command_list_->SetGraphicsRootShaderResourceView(3, scene_gpu_->material_buffer->GetGPUVirtualAddress());
        command_list_->SetGraphicsRootDescriptorTable(4, srv_heap_->GetGPUDescriptorHandleForHeapStart());

        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list_->IASetVertexBuffers(0, 1, &scene_gpu_->vertex_buffer_view);
        command_list_->IASetIndexBuffer(&scene_gpu_->index_buffer_view);

        for (const auto& batch : scene_cpu_->batches) {
            const auto& material = scene_cpu_->materials[batch.material_index];
            const auto& mesh = scene_cpu_->meshes[batch.mesh_index];

            command_list_->SetGraphicsRoot32BitConstant(2, batch.object_index, 0);

            command_list_->DrawIndexedInstanced(mesh.index_count, batch.object_count,
                mesh.index_start, mesh.vertex_start, 0);
        }

        dxutl::transition_resource(command_list_.Get(), render_targets_[frame_index_].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 0);
        Utils::throw_if_failed(command_list_->Close(), "command list clonse on framne end");

        ID3D12CommandList* command_lists[] = { command_list_.Get() };
        command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);
        Utils::throw_if_failed(swapchain_->Present(0, DXGI_PRESENT_ALLOW_TEARING), "swapchain present");
    }


    UINT RendererForward::srv_descriptor_count() const {
        return program_arguments_->texture_count;
    }

    void RendererForward::create_shader_resources() {
        create_texture_srv_descriptors(srv_heap_->GetCPUDescriptorHandleForHeapStart());
    }

    void RendererForward::create_root_signature() {

        // b0 (constant buffer)
        D3D12_DESCRIPTOR_RANGE texture_range{};
        texture_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        texture_range.NumDescriptors = program_arguments_->texture_count;
        texture_range.BaseShaderRegister = 8;
        texture_range.RegisterSpace = 0;
        texture_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER root_parameters[5]{};
        root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        root_parameters[0].Descriptor.ShaderRegister = 0;
        root_parameters[0].Descriptor.RegisterSpace = 0;
        root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // t0 (per instance structuredbuffer)
        root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        root_parameters[1].Descriptor.ShaderRegister = 0;
        root_parameters[1].Descriptor.RegisterSpace = 0;
        root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // b1 (instance start offset constant)
        root_parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        root_parameters[2].Constants.ShaderRegister = 1;
        root_parameters[2].Constants.RegisterSpace = 0;
        root_parameters[2].Constants.Num32BitValues = 1;
        root_parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        root_parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        root_parameters[3].Descriptor.ShaderRegister = 1;
        root_parameters[3].Descriptor.RegisterSpace = 0;
        root_parameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        root_parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_parameters[4].DescriptorTable.NumDescriptorRanges = 1;
        root_parameters[4].DescriptorTable.pDescriptorRanges = &texture_range;
        root_parameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC root_sig_desc{};
        root_sig_desc.NumParameters = _countof(root_parameters);
        root_sig_desc.pParameters = root_parameters;
        root_sig_desc.NumStaticSamplers = 0;
        root_sig_desc.pStaticSamplers = nullptr;
        root_sig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;

        Utils::throw_if_failed(D3D12SerializeRootSignature(
            &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error), "create root signature");

        Utils::throw_if_failed(device_->CreateRootSignature(
            0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_)), "create root signature");
    }
    
    void RendererForward::create_pso()
    {
        Microsoft::WRL::ComPtr<ID3DBlob> vertex_shader;
        Microsoft::WRL::ComPtr<ID3DBlob> pixel_shader;

        std::string texture_count_define = std::to_string(program_arguments_->texture_count);
        std::string texture_sampling_count_define = std::to_string(program_arguments_->texture_sampling_count);
        std::string texture_size_define = std::to_string(program_arguments_->texture_size);
        std::string alu_calc_count_define = std::to_string(program_arguments_->alu_calc_count);
        D3D_SHADER_MACRO workload_defines[] =
        {
            { "TEXTURE_COUNT", texture_count_define.c_str() },
            { "TEXTURE_SAMPLING_COUNT", texture_sampling_count_define.c_str() },
            { "TEXTURE_SIZE", texture_size_define.c_str() },
            { "ALU_CALC_COUNT", alu_calc_count_define.c_str() },
            { nullptr, nullptr }
        };

        vertex_shader = dxutl::compile_shader(L"assets/shaders/forward_VS.hlsl", "vs_5_0");
        pixel_shader = dxutl::compile_shader(L"assets/shaders/forward_PS.hlsl", "ps_5_0", "main", workload_defines);

        D3D12_INPUT_ELEMENT_DESC input_layout[] =
        {
            {
                "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0,
                32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            }
        };

        D3D12_RASTERIZER_DESC rasterizer_desc{};
        rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizer_desc.CullMode = D3D12_CULL_MODE_BACK;
        rasterizer_desc.FrontCounterClockwise = FALSE;
        rasterizer_desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizer_desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizer_desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizer_desc.DepthClipEnable = TRUE;
        rasterizer_desc.MultisampleEnable = FALSE;
        rasterizer_desc.AntialiasedLineEnable = FALSE;
        rasterizer_desc.ForcedSampleCount = 0;
        rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        D3D12_BLEND_DESC blend_desc{};
        blend_desc.AlphaToCoverageEnable = FALSE;
        blend_desc.IndependentBlendEnable = FALSE;

        const D3D12_RENDER_TARGET_BLEND_DESC default_render_target_blend_desc =
        {
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

        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
            blend_desc.RenderTarget[i] = default_render_target_blend_desc;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
        pso_desc.InputLayout = { input_layout, _countof(input_layout) };
        pso_desc.pRootSignature = root_signature_.Get();
        pso_desc.VS = {
            vertex_shader->GetBufferPointer(),
            vertex_shader->GetBufferSize()
        };
        pso_desc.PS = {
            pixel_shader->GetBufferPointer(),
            pixel_shader->GetBufferSize()
        };
        pso_desc.RasterizerState = rasterizer_desc;
        pso_desc.BlendState = blend_desc;
        pso_desc.DepthStencilState.DepthEnable = TRUE;
        pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        pso_desc.DSVFormat = DEPTH_STENCIL_FORMAT_;
        pso_desc.DepthStencilState.StencilEnable = FALSE;
        pso_desc.SampleMask = UINT_MAX;
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.NumRenderTargets = 1;
        pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso_desc.SampleDesc.Count = 1;

        Utils::throw_if_failed(device_->CreateGraphicsPipelineState(
            &pso_desc,
            IID_PPV_ARGS(&pipeline_state_)), "create pso");
    }
}
