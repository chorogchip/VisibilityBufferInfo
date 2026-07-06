#include "render/RendererForward.h"

#include <d3d12.h>

#include "util/Utils.h"
#include "util/GraphicsUtils.h"

namespace rndr {

    using Microsoft::WRL::ComPtr;

    void RendererForward::render()
    {
        Utils::throw_if_failed(command_allocator_[frame_index_]->Reset(), "reset command allocator");

        Utils::throw_if_failed(command_list_->Reset(
            command_allocator_[frame_index_].Get(),
            pipeline_state_.Get()), "command list reset on render start");

        const UINT timestamp_base = frame_index_ * GpuFrameTime<FRAME_COUNT>::TIMESTAMP_COUNT_PER_FRAME;
        const UINT timestamp_start_index = timestamp_base + GpuFrameTime<FRAME_COUNT>::TIMESTAMP_START;
        const UINT timestamp_end_index = timestamp_base + GpuFrameTime<FRAME_COUNT>::TIMESTAMP_END;

        command_list_->EndQuery(frame_time.timestamp_query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, timestamp_start_index);

        D3D12_RESOURCE_BARRIER barrier_to_rt{};
        barrier_to_rt.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier_to_rt.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier_to_rt.Transition.pResource = render_targets_[frame_index_].Get();
        barrier_to_rt.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier_to_rt.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier_to_rt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        command_list_->ResourceBarrier(1, &barrier_to_rt);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle =
            rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        rtv_handle.ptr += static_cast<SIZE_T>(frame_index_) * rtv_descriptor_size_;

        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
        command_list_->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);

        D3D12_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(width_);
        viewport.Height = static_cast<float>(height_);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        D3D12_RECT scissor_rect{};
        scissor_rect.left = 0;
        scissor_rect.top = 0;
        scissor_rect.right = static_cast<LONG>(width_);
        scissor_rect.bottom = static_cast<LONG>(height_);

        command_list_->RSSetViewports(1, &viewport);
        command_list_->RSSetScissorRects(1, &scissor_rect);

        const float clear_color[] = { 0.1f, 0.1f, 0.15f, 1.0f };
        command_list_->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
        command_list_->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        command_list_->SetGraphicsRootSignature(root_signature_.Get());
        command_list_->SetGraphicsRootConstantBufferView(0, buf_constant_->GetGPUVirtualAddress());
        command_list_->SetGraphicsRootShaderResourceView(1, buf_instance_->GetGPUVirtualAddress());

        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list_->IASetVertexBuffers(0, 1, &scene_resource_->vertex_buffer_view);
        command_list_->IASetIndexBuffer(&scene_resource_->index_buffer_view);

        int inst_num = 0;
        for (const auto& instance : scene_raw_->instances) {
            const auto& geom_handle = scene_resource_->geometries_handles[instance.geometry_id];

            command_list_->SetGraphicsRoot32BitConstant(2, inst_num, 0);

            command_list_->DrawIndexedInstanced(geom_handle.index_count, 1,
                geom_handle.start_index_offset, geom_handle.base_vertex_offset, 0);

            ++inst_num;
        }

        D3D12_RESOURCE_BARRIER barrier_to_present{};
        barrier_to_present.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier_to_present.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier_to_present.Transition.pResource = render_targets_[frame_index_].Get();
        barrier_to_present.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier_to_present.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        barrier_to_present.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        command_list_->ResourceBarrier(1, &barrier_to_present);

        command_list_->EndQuery(frame_time.timestamp_query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, timestamp_end_index);
        command_list_->ResolveQueryData(
            frame_time.timestamp_query_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            timestamp_start_index, GpuFrameTime<FRAME_COUNT>::TIMESTAMP_COUNT_PER_FRAME,
            frame_time.timestamp_readback_buffer_.Get(), sizeof(UINT64) * timestamp_base);

        frame_time.timestamp_frame_valid_[frame_index_] = true;

        Utils::throw_if_failed(command_list_->Close(), "command list clonse on framne end");

        ID3D12CommandList* command_lists[] = { command_list_.Get() };

        command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);
        Utils::throw_if_failed(swapchain_->Present(0, DXGI_PRESENT_ALLOW_TEARING), "swapchain present");

        this->move_to_next_frame();
    }

    void RendererForward::create_root_signature() {

        // b0 (constant buffer)
        D3D12_ROOT_PARAMETER root_parameters[3]{};
        root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        root_parameters[0].Descriptor.ShaderRegister = 0;
        root_parameters[0].Descriptor.RegisterSpace = 0;
        root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // t0 (per instance structuredbuffer)
        root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        root_parameters[1].Descriptor.ShaderRegister = 0;
        root_parameters[1].Descriptor.RegisterSpace = 0;
        root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // b1 (instance index, root constant)
        root_parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        root_parameters[2].Constants.ShaderRegister = 1;
        root_parameters[2].Constants.RegisterSpace = 0;
        root_parameters[2].Constants.Num32BitValues = 1;
        root_parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_ROOT_SIGNATURE_DESC root_sig_desc{};
        root_sig_desc.NumParameters = _countof(root_parameters);
        root_sig_desc.pParameters = root_parameters;
        root_sig_desc.NumStaticSamplers = 0;
        root_sig_desc.pStaticSamplers = nullptr;
        root_sig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;

        Utils::throw_if_failed(D3D12SerializeRootSignature(
            &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error), "create root signature");

        Utils::throw_if_failed(device_->CreateRootSignature(
            0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_)), "create root signature");
    }
    
    void RendererForward::create_pso()
    {
        ComPtr<ID3DBlob> vertex_shader;
        ComPtr<ID3DBlob> pixel_shader;

        GraphicsUtils::compile_shader(&vertex_shader, L"assets/shaders/forward_VS.hlsl", "vs_5_0");
        GraphicsUtils::compile_shader(&pixel_shader, L"assets/shaders/forward_PS.hlsl", "ps_5_0");

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
        pso_desc.DSVFormat = depth_stencil_format_;
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