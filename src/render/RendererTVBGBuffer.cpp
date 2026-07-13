#include "render/RendererTVBGBuffer.h"

#include <d3d12.h>
#include <string>

#include "util/Utils.h"
#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "dx_util/DescriptorUtils.h"

namespace rndr {

    void RendererTVBGBuffer::make_programresult(util::ProgramResult& result) {
        result.renderer_name = "VisBuf";
        result.pass_name_0 = "visibility";
        result.pass_name_1 = "gbuffer";
        result.pass_name_2 = "lighting";
        result.pass_name_3 = "total";
    }

    void RendererTVBGBuffer::create_pass_resources() {

        util::Logger::g_logger.assert_with_log(
            program_arguments_->gbuffer_cnt > 0,
            "gbuffer count must > 0 in deferred"
        );
        util::Logger::g_logger.assert_with_log(
            program_arguments_->gbuffer_cnt <= 8,
            "gbuffer count must < 8 in deferred"
        );

        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = DXGI_FORMAT_R32G32_UINT;
        clear_value.Color[0] = 0.0f;
        clear_value.Color[1] = 0.0f;
        clear_value.Color[2] = 0.0f;
        clear_value.Color[3] = 0.0f;

        vis_buffer_ = dxutl::create_texture2d(device_.Get(), width_, height_,
            DXGI_FORMAT_R32G32_UINT,
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            &clear_value);

        for (uint32_t i = 0; i < program_arguments_->gbuffer_cnt; ++i) {

            gbuffers_.emplace_back();

            D3D12_CLEAR_VALUE gbuffer_clear_value{};
            gbuffer_clear_value.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            gbuffer_clear_value.Color[0] = CLEAR_COLOR_[0];
            gbuffer_clear_value.Color[1] = CLEAR_COLOR_[1];
            gbuffer_clear_value.Color[2] = CLEAR_COLOR_[2];
            gbuffer_clear_value.Color[3] = CLEAR_COLOR_[3];

            gbuffers_.back() = dxutl::create_texture2d(device_.Get(), width_, height_,
                DXGI_FORMAT_R32G32B32A32_FLOAT,
                D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                &gbuffer_clear_value);
        }
    }

    void RendererTVBGBuffer::render_() {

        Utils::throw_if_failed(command_allocator_[frame_index_]->Reset(), "reset command allocator");
        Utils::throw_if_failed(command_list_->Reset(command_allocator_[frame_index_].Get(),
            pipeline_state_.Get()), "command list reset on render start");
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 0);

        this->copy_camera_data();

        dxutl::transition_resource(command_list_.Get(), vis_buffer_.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

        command_list_->RSSetViewports(1, &viewport_);
        command_list_->RSSetScissorRects(1, &scissor_rect_);

        // visibility pass

        command_list_->SetGraphicsRootSignature(root_signature_.Get());
        command_list_->SetGraphicsRootConstantBufferView(0, buf_constant_[frame_index_]->GetGPUVirtualAddress());
        command_list_->SetGraphicsRootShaderResourceView(1, scene_gpu_->object_buffer->GetGPUVirtualAddress());

        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list_->IASetVertexBuffers(0, 1, &scene_gpu_->vertex_buffer_view);
        command_list_->IASetIndexBuffer(&scene_gpu_->index_buffer_view);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle =
            rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        rtv_handle.ptr += static_cast<SIZE_T>(FRAME_COUNT) * rtv_descriptor_size_;
        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
        command_list_->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);

        float clear_color[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
        command_list_->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

        command_list_->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        for (const auto& batch : scene_cpu_->batches) {
            const auto& material = scene_cpu_->materials[batch.material_index];
            const auto& mesh = scene_cpu_->meshes[batch.mesh_index];

            command_list_->SetGraphicsRoot32BitConstant(2, batch.object_index, 0);

            command_list_->DrawIndexedInstanced(mesh.index_count, batch.object_count,
                mesh.index_start, mesh.vertex_start, 0);
        }

        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 0);
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 1);

        // gbuffer pass

        command_list_->SetPipelineState(pso_gbuffer_.Get());
        command_list_->SetGraphicsRootSignature(root_signature_gbuffer_.Get());
        ID3D12DescriptorHeap* heaps[] = { srv_heap_.Get() };
        command_list_->SetDescriptorHeaps(_countof(heaps), heaps);
        command_list_->SetGraphicsRootConstantBufferView(0, buf_constant_[frame_index_]->GetGPUVirtualAddress());
        command_list_->SetGraphicsRootDescriptorTable(1, srv_heap_->GetGPUDescriptorHandleForHeapStart());
        D3D12_GPU_DESCRIPTOR_HANDLE texture_handle = srv_heap_->GetGPUDescriptorHandleForHeapStart();
        texture_handle.ptr += static_cast<SIZE_T>(6 + program_arguments_->gbuffer_cnt) * srv_descriptor_size_;
        command_list_->SetGraphicsRootDescriptorTable(2, texture_handle);

        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        dxutl::transition_resource(command_list_.Get(), vis_buffer_.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        for (UINT i = 0; i < program_arguments_->gbuffer_cnt; ++i)
            dxutl::transition_resource(command_list_.Get(), gbuffers_[i].Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

        rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        rtv_handle.ptr += static_cast<SIZE_T>(FRAME_COUNT + 1) * rtv_descriptor_size_;

        command_list_->OMSetRenderTargets(program_arguments_->gbuffer_cnt, &rtv_handle, TRUE, nullptr);

        for (UINT i = 0; i < program_arguments_->gbuffer_cnt; ++i) {
            command_list_->ClearRenderTargetView(rtv_handle, CLEAR_COLOR_, 0, nullptr);
            rtv_handle.ptr += rtv_descriptor_size_;
        }

        command_list_->DrawInstanced(3, 1, 0, 0);

        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 1);
        frame_time_.start_timestamp(command_list_.Get(), frame_index_, 2);

        // lighting pass

        command_list_->SetPipelineState(pso_lighting_.Get());
        command_list_->SetGraphicsRootSignature(root_signature_lighting_.Get());
        command_list_->SetDescriptorHeaps(_countof(heaps), heaps);
        D3D12_GPU_DESCRIPTOR_HANDLE gbuffer_handle = srv_heap_->GetGPUDescriptorHandleForHeapStart();
        gbuffer_handle.ptr += static_cast<SIZE_T>(6) * srv_descriptor_size_;
        command_list_->SetGraphicsRootDescriptorTable(0, gbuffer_handle);

        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        dxutl::transition_resource(command_list_.Get(), render_targets_[frame_index_].Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        for (UINT i = 0; i < program_arguments_->gbuffer_cnt; ++i)
            dxutl::transition_resource(command_list_.Get(), gbuffers_[i].Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        rtv_handle.ptr += static_cast<SIZE_T>(frame_index_) * rtv_descriptor_size_;

        command_list_->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

        command_list_->ClearRenderTargetView(rtv_handle, CLEAR_COLOR_, 0, nullptr);

        command_list_->DrawInstanced(3, 1, 0, 0);

        dxutl::transition_resource(command_list_.Get(), render_targets_[frame_index_].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        frame_time_.end_timestamp(command_list_.Get(), frame_index_, 2);
        Utils::throw_if_failed(command_list_->Close(), "command list clonse on framne end");

        ID3D12CommandList* command_lists[] = { command_list_.Get() };
        command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);
        Utils::throw_if_failed(swapchain_->Present(0, DXGI_PRESENT_ALLOW_TEARING), "swapchain present");
    }


    UINT RendererTVBGBuffer::rtv_descriptor_count() const {
        return FRAME_COUNT + 1 + program_arguments_->gbuffer_cnt;
    }

    void RendererTVBGBuffer::create_extra_render_target_views(D3D12_CPU_DESCRIPTOR_HANDLE next_rtv_handle) {
        device_->CreateRenderTargetView(vis_buffer_.Get(), nullptr, next_rtv_handle);
        next_rtv_handle.ptr += rtv_descriptor_size_;

        for (UINT i = 0; i < program_arguments_->gbuffer_cnt; ++i) {
            device_->CreateRenderTargetView(gbuffers_[i].Get(), nullptr, next_rtv_handle);
            next_rtv_handle.ptr += rtv_descriptor_size_;
        }
    }

    UINT RendererTVBGBuffer::srv_descriptor_count() const {
        return 6 + program_arguments_->gbuffer_cnt + program_arguments_->texture_count;
    }

    void RendererTVBGBuffer::create_shader_resources() {

        struct Mesh {
            uint32_t vertex_start;
            uint32_t index_start;
        };
        std::vector<Mesh> meshes;
        meshes.reserve(scene_cpu_->meshes.size());
        for (const auto& mesh : scene_cpu_->meshes)
            meshes.push_back({ mesh.vertex_start, mesh.index_start });

        const size_t mesh_buf_sz = meshes.size() * sizeof(meshes[0]);

        Microsoft::WRL::ComPtr<ID3D12Resource> upload_buf;

        upload_buf = dxutl::create_buffer(device_.Get(), mesh_buf_sz, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        mesh_buffer_ = dxutl::create_buffer(device_.Get(), mesh_buf_sz, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);

        dxutl::copy_to_upload_buffer(upload_buf.Get(), meshes.data(), mesh_buf_sz);

        Utils::throw_if_failed(command_list_->Reset(
            command_allocator_[frame_index_].Get(), pipeline_state_.Get()));

        command_list_->CopyBufferRegion(mesh_buffer_.Get(), 0, upload_buf.Get(), 0, mesh_buf_sz);

        // COMMON -> COPY_DEST: implicit transition
        dxutl::transition_resource(command_list_.Get(), mesh_buffer_.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dxutl::transition_resource(command_list_.Get(), scene_gpu_->vertex_buffer.Get(),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dxutl::transition_resource(command_list_.Get(), scene_gpu_->index_buffer.Get(),
            D3D12_RESOURCE_STATE_INDEX_BUFFER,
            D3D12_RESOURCE_STATE_INDEX_BUFFER | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        // this is for later work
        dxutl::transition_resource(command_list_.Get(), scene_gpu_->object_buffer.Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

        Utils::throw_if_failed(command_list_->Close(), "close list on resource creation");
        ID3D12CommandList* command_lists[] = { command_list_.Get() };
        command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);
        fence_.wait_for_gpu();


        assert(scene_cpu_->vertices.size() <= UINT_MAX);
        assert(scene_cpu_->indices.size() <= UINT_MAX);
        assert(meshes.size() <= UINT_MAX);
        assert(scene_cpu_->objects.size() <= UINT_MAX);
        assert(scene_cpu_->materials.size() <= UINT_MAX);

        D3D12_CPU_DESCRIPTOR_HANDLE srv_handle =
            srv_heap_->GetCPUDescriptorHandleForHeapStart();

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = DXGI_FORMAT_R32G32_UINT;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;

        device_->CreateShaderResourceView(vis_buffer_.Get(), &srv_desc, srv_handle);
        srv_handle.ptr += srv_descriptor_size_;

        srv_desc.Format = DXGI_FORMAT_UNKNOWN;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Buffer.FirstElement = 0;
        srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srv_desc.Buffer.StructureByteStride = sizeof(scene_cpu_->vertices[0]);
        srv_desc.Buffer.NumElements = static_cast<UINT>(scene_cpu_->vertices.size());
        device_->CreateShaderResourceView(scene_gpu_->vertex_buffer.Get(), &srv_desc, srv_handle);
        srv_handle.ptr += srv_descriptor_size_;

        srv_desc.Buffer.StructureByteStride = sizeof(scene_cpu_->indices[0]);
        srv_desc.Buffer.NumElements = static_cast<UINT>(scene_cpu_->indices.size());
        device_->CreateShaderResourceView(scene_gpu_->index_buffer.Get(), &srv_desc, srv_handle);
        srv_handle.ptr += srv_descriptor_size_;

        srv_desc.Buffer.StructureByteStride = sizeof(Mesh);
        srv_desc.Buffer.NumElements = static_cast<UINT>(meshes.size());
        device_->CreateShaderResourceView(mesh_buffer_.Get(), &srv_desc, srv_handle);
        srv_handle.ptr += srv_descriptor_size_;

        srv_desc.Buffer.StructureByteStride = sizeof(scene_cpu_->objects[0]);
        srv_desc.Buffer.NumElements = static_cast<UINT>(scene_cpu_->objects.size());
        device_->CreateShaderResourceView(scene_gpu_->object_buffer.Get(), &srv_desc, srv_handle);
        srv_handle.ptr += srv_descriptor_size_;

        srv_desc.Buffer.StructureByteStride = sizeof(float) * 4;
        srv_desc.Buffer.NumElements = static_cast<UINT>(scene_cpu_->materials.size());
        device_->CreateShaderResourceView(scene_gpu_->material_buffer.Get(), &srv_desc, srv_handle);
        srv_handle.ptr += srv_descriptor_size_;

        srv_desc = D3D12_SHADER_RESOURCE_VIEW_DESC{};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;

        for (UINT i = 0; i < program_arguments_->gbuffer_cnt; ++i) {
            device_->CreateShaderResourceView(gbuffers_[i].Get(), &srv_desc, srv_handle);
            srv_handle.ptr += srv_descriptor_size_;
        }

        create_texture_srv_descriptors(srv_handle);
    }

    void RendererTVBGBuffer::create_root_signature() {

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

        // b1 (instance start offset constant)
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

        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;

        Utils::throw_if_failed(D3D12SerializeRootSignature(
            &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error),
            "create root signature");

        Utils::throw_if_failed(device_->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(root_signature_.ReleaseAndGetAddressOf())),
            "create root signature");


        D3D12_DESCRIPTOR_RANGE srv_range{};
        srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srv_range.NumDescriptors = 6;
        srv_range.BaseShaderRegister = 0;
        srv_range.RegisterSpace = 0;
        srv_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE texture_range{};
        texture_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        texture_range.NumDescriptors = program_arguments_->texture_count;
        texture_range.BaseShaderRegister = 8;
        texture_range.RegisterSpace = 0;
        texture_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER root_parameter_gbuffer[3]{};
        // b0 (constant buffer)
        root_parameter_gbuffer[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        root_parameter_gbuffer[0].Descriptor.ShaderRegister = 0;
        root_parameter_gbuffer[0].Descriptor.RegisterSpace = 0;
        root_parameter_gbuffer[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        // t0~t4 (shader resource)
        root_parameter_gbuffer[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_parameter_gbuffer[1].DescriptorTable.NumDescriptorRanges = 1;
        root_parameter_gbuffer[1].DescriptorTable.pDescriptorRanges = &srv_range;
        root_parameter_gbuffer[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        root_parameter_gbuffer[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_parameter_gbuffer[2].DescriptorTable.NumDescriptorRanges = 1;
        root_parameter_gbuffer[2].DescriptorTable.pDescriptorRanges = &texture_range;
        root_parameter_gbuffer[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        root_sig_desc.NumParameters = _countof(root_parameter_gbuffer);
        root_sig_desc.pParameters = root_parameter_gbuffer;
        root_sig_desc.NumStaticSamplers = 0;
        root_sig_desc.pStaticSamplers = nullptr;
        root_sig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        // later create samplers

        Utils::throw_if_failed(D3D12SerializeRootSignature(
            &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error), "create root signature");

        Utils::throw_if_failed(device_->CreateRootSignature(
            0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_gbuffer_)), "create root signature");

        D3D12_DESCRIPTOR_RANGE gbuffer_range{};
        gbuffer_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        gbuffer_range.NumDescriptors = program_arguments_->gbuffer_cnt;
        gbuffer_range.BaseShaderRegister = 0;
        gbuffer_range.RegisterSpace = 0;
        gbuffer_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER root_parameter_lighting[1]{};
        root_parameter_lighting[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_parameter_lighting[0].DescriptorTable.NumDescriptorRanges = 1;
        root_parameter_lighting[0].DescriptorTable.pDescriptorRanges = &gbuffer_range;
        root_parameter_lighting[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        root_sig_desc.NumParameters = _countof(root_parameter_lighting);
        root_sig_desc.pParameters = root_parameter_lighting;

        Utils::throw_if_failed(D3D12SerializeRootSignature(
            &root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error), "create root signature");

        Utils::throw_if_failed(device_->CreateRootSignature(
            0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_lighting_)), "create root signature");
    }

    void RendererTVBGBuffer::create_pso() {

        Microsoft::WRL::ComPtr<ID3DBlob> vertex_shader_visibility =
            dxutl::compile_shader(L"assets/shaders/TVB_visibility_VS.hlsl", "vs_5_0", "main", *program_arguments_);
        Microsoft::WRL::ComPtr<ID3DBlob> pixel_shader_visibility =
            dxutl::compile_shader(L"assets/shaders/TVB_visibility_PS.hlsl", "ps_5_0", "main", *program_arguments_);
        Microsoft::WRL::ComPtr<ID3DBlob> pixel_shader_gbuffer =
            dxutl::compile_shader(L"assets/shaders/TVB_gbuffer_PS.hlsl", "ps_5_0", "main", *program_arguments_);
        Microsoft::WRL::ComPtr<ID3DBlob> vertex_shader_lighting =
            dxutl::compile_shader(L"assets/shaders/TVB_lighting_VS.hlsl", "vs_5_0", "main", *program_arguments_);
        Microsoft::WRL::ComPtr<ID3DBlob> pixel_shader_lighting =
            dxutl::compile_shader(L"assets/shaders/deferred_lighting_PS.hlsl", "ps_5_0", "main", *program_arguments_);

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
        pso_desc.InputLayout = dxutl::get_default_input_layout_desc();
        pso_desc.pRootSignature = root_signature_.Get();
        pso_desc.VS = {
            vertex_shader_visibility->GetBufferPointer(),
            vertex_shader_visibility->GetBufferSize()
        };
        pso_desc.PS = {
            pixel_shader_visibility->GetBufferPointer(),
            pixel_shader_visibility->GetBufferSize()
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
        pso_desc.RTVFormats[0] = DXGI_FORMAT_R32G32_UINT;
        pso_desc.SampleDesc.Count = 1;

        Utils::throw_if_failed(device_->CreateGraphicsPipelineState(
            &pso_desc,
            IID_PPV_ARGS(&pipeline_state_)), "create pso");

        pso_desc.InputLayout = { nullptr, 0 };
        pso_desc.pRootSignature = root_signature_gbuffer_.Get();
        pso_desc.VS = {
            vertex_shader_lighting->GetBufferPointer(),
            vertex_shader_lighting->GetBufferSize()
        };
        pso_desc.PS = {
            pixel_shader_gbuffer->GetBufferPointer(),
            pixel_shader_gbuffer->GetBufferSize()
        };
        pso_desc.DepthStencilState.DepthEnable = FALSE;
        pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
        pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        pso_desc.NumRenderTargets = program_arguments_->gbuffer_cnt;
        for (UINT i = 0; i < program_arguments_->gbuffer_cnt; ++i)
            pso_desc.RTVFormats[i] = DXGI_FORMAT_R32G32B32A32_FLOAT;
        for (UINT i = program_arguments_->gbuffer_cnt; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
            pso_desc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;

        Utils::throw_if_failed(device_->CreateGraphicsPipelineState(
            &pso_desc,
            IID_PPV_ARGS(&pso_gbuffer_)), "create pso");

        pso_desc.pRootSignature = root_signature_lighting_.Get();
        pso_desc.PS = {
            pixel_shader_lighting->GetBufferPointer(),
            pixel_shader_lighting->GetBufferSize()
        };
        pso_desc.NumRenderTargets = 1;
        pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        for (UINT i = 1; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
            pso_desc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;

        Utils::throw_if_failed(device_->CreateGraphicsPipelineState(
            &pso_desc,
            IID_PPV_ARGS(&pso_lighting_)), "create pso");
    }
}
