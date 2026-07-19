#include "render/pass/PassGBuffer.h"

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "render/RootParameter.h"
#include "render/pass/PassDescriptorRequests.h"
#include "render/renderer/RendererBase.h"

namespace rndr {

    void PassGBuffer::init(ID3D12Device* device, const util::ProgramArgument& arguments,
        const PassGBufferResources& resources, bool use_prepass_depth) {
        resources_ = resources;
        use_prepass_depth_ = use_prepass_depth;

        auto& frame = RendererBase::get_resource_manager().frame;
        for (UINT i = 0; i < resources_.gbuffer_count; ++i)
            frame.request_rtv(static_cast<eng::ResourceManagerFrame::EnumRTV>(
                static_cast<UINT>(eng::ResourceManagerFrame::EnumRTV::BENCH_GBUFFER_0) + i),
                resources_.gbuffers[i]);
        frame.request_dsv(use_prepass_depth_
            ? eng::ResourceManagerFrame::EnumDSV::DEPTH_READ_ONLY
            : eng::ResourceManagerFrame::EnumDSV::DEPTH, resources_.depth);
        request_material_textures(RendererBase::get_resource_manager().shader, resources_.scene);
        D3D12_SHADER_RESOURCE_VIEW_DESC gbuffer_desc{};
        gbuffer_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        gbuffer_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        gbuffer_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        gbuffer_desc.Texture2D.MipLevels = 1;
        for (UINT i = 0; i < resources_.gbuffer_count; ++i)
            RendererBase::get_resource_manager().shader.request(
                eng::ResourceManagerShader::EnumDescPos::BENCH_GBUFFER_0,
                resources_.gbuffers[i], &gbuffer_desc, i);

        auto vs = dxutl::compile_shader(L"assets/shaders/deferred_geometry_VS.hlsl", "vs_5_0", "main", arguments);
        auto ps = dxutl::compile_shader(L"assets/shaders/deferred_geometry_PS.hlsl", "ps_5_0", "main", arguments);
        pso_.init(device);
        pso_.set_texture_count(arguments.texture_count);
        pso_.set_shaders(vs.Get(), ps.Get());
        pso_.set_render_targets(resources_.gbuffer_count, DXGI_FORMAT_R32G32B32A32_FLOAT);
        if (use_prepass_depth_)
            pso_.set_depth_equal();
        pso_.build();
    }

    void PassGBuffer::render(ID3D12GraphicsCommandList* command_list, UINT frame_index,
        const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor_rect) {
        auto& managers = RendererBase::get_resource_manager();
        for (UINT i = 0; i < resources_.gbuffer_count; ++i)
            dxutl::transition_resource(command_list, resources_.gbuffers[i],
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        if (use_prepass_depth_)
            dxutl::transition_resource(command_list, resources_.depth,
                D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_DEPTH_READ);

        command_list->SetPipelineState(pso_.get());
        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        ID3D12DescriptorHeap* heaps[] = { managers.shader.get(), resources_.sampler_heap };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);
        command_list->SetGraphicsRootConstantBufferView(root_param(EnumRootParamScene::FRAME_CONSTANT),
            resources_.constant_buffers[frame_index]->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(root_param(EnumRootParamScene::BENCH_INSTANCE_BUFFER),
            resources_.scene.instance_buffer->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(root_param(EnumRootParamScene::BENCH_MATERIAL_BUFFER),
            resources_.scene.material_buffer->GetGPUVirtualAddress());
        command_list->SetGraphicsRootDescriptorTable(root_param(EnumRootParamScene::BENCH_MATERIAL_TEXTURE),
            managers.shader.get_gpu_adr(eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_TEXTURE_BEGIN));
        command_list->SetGraphicsRootDescriptorTable(root_param(EnumRootParamScene::BENCH_MATERIAL_SAMPLER),
            resources_.sampler_heap->GetGPUDescriptorHandleForHeapStart());

        D3D12_CPU_DESCRIPTOR_HANDLE rtvs[8]{};
        for (UINT i = 0; i < resources_.gbuffer_count; ++i) {
            rtvs[i] = managers.frame.get_rtv(static_cast<eng::ResourceManagerFrame::EnumRTV>(
                static_cast<UINT>(eng::ResourceManagerFrame::EnumRTV::BENCH_GBUFFER_0) + i));
            constexpr float clear[] = { .1f, .1f, .15f, 1.f };
            command_list->ClearRenderTargetView(rtvs[i], clear, 0, nullptr);
        }
        const auto dsv = managers.frame.get_dsv(use_prepass_depth_
            ? eng::ResourceManagerFrame::EnumDSV::DEPTH_READ_ONLY
            : eng::ResourceManagerFrame::EnumDSV::DEPTH);
        command_list->OMSetRenderTargets(resources_.gbuffer_count, rtvs, FALSE, &dsv);
        if (!use_prepass_depth_)
            command_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetVertexBuffers(0, 1, &resources_.scene.vertex_buffer_view);
        command_list->IASetIndexBuffer(&resources_.scene.index_buffer_view);
        for (const auto& batch : resources_.scene.cpu->batches) {
            const auto& mesh = resources_.scene.cpu->meshes[batch.mesh_index];
            command_list->SetGraphicsRoot32BitConstant(root_param(EnumRootParamScene::DRAW_CONSTANT),
                batch.object_index, 0);
            command_list->DrawIndexedInstanced(mesh.index_count, batch.object_count,
                mesh.index_start, mesh.vertex_start, 0);
        }
    }

}
