#include "render/pass/PassForward.h"

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "render/renderer/RendererBase.h"
#include "render/RootParameter.h"
#include "render/pass/PassDescriptorRequests.h"

namespace rndr {

    void PassForward::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassForwardResources& resources,
        bool use_prepass_depth) {
        resources_ = resources;
        use_prepass_depth_ = use_prepass_depth;

        auto& managers = RendererBase::get_resource_manager();
        managers.frame.request_rtv(eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0, resources_.back_buffers[0]);
        managers.frame.request_rtv(eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_1, resources_.back_buffers[1]);
        managers.frame.request_dsv(eng::ResourceManagerFrame::EnumDSV::DEPTH, resources_.depth);
        if (use_prepass_depth_)
            managers.frame.request_dsv(eng::ResourceManagerFrame::EnumDSV::DEPTH_READ_ONLY, resources_.depth);
        request_material_textures(managers.shader, resources_.scene);

        auto vertex_shader = dxutl::compile_shader(
            L"assets/shaders/forward_VS.hlsl", "vs_5_0", "main", arguments);
        auto pixel_shader = dxutl::compile_shader(
            L"assets/shaders/forward_PS.hlsl", "ps_5_0", "main", arguments);

        pso_.init(device);
        pso_.set_texture_count(arguments.texture_count);
        pso_.set_shaders(vertex_shader.Get(), pixel_shader.Get());
        if (use_prepass_depth_)
            pso_.set_depth_equal();
        pso_.build();
    }

    void PassForward::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {
        auto& managers = RendererBase::get_resource_manager();

        command_list->SetPipelineState(pso_.get());
        dxutl::transition_resource(command_list, resources_.back_buffers[frame_index],
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        const auto rtv = managers.frame.get_rtv(frame_index == 0
            ? eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0
            : eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_1);
        if (use_prepass_depth_)
            dxutl::transition_resource(command_list, resources_.depth,
                D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_DEPTH_READ);

        const auto dsv = managers.frame.get_dsv(use_prepass_depth_
            ? eng::ResourceManagerFrame::EnumDSV::DEPTH_READ_ONLY
            : eng::ResourceManagerFrame::EnumDSV::DEPTH);
        command_list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);

        constexpr float clear_color[] = { 0.1f, 0.1f, 0.15f, 1.f };
        command_list->ClearRenderTargetView(rtv, clear_color, 0, nullptr);
        if (!use_prepass_depth_)
            command_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        ID3D12DescriptorHeap* heaps[] = { managers.shader.get(), resources_.sampler_heap };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);
        command_list->SetGraphicsRootConstantBufferView(
            root_param(EnumRootParamScene::FRAME_CONSTANT),
            resources_.constant_buffers[frame_index]->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            root_param(EnumRootParamScene::BENCH_INSTANCE_BUFFER),
            resources_.scene.instance_buffer->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            root_param(EnumRootParamScene::BENCH_MATERIAL_BUFFER),
            resources_.scene.material_buffer->GetGPUVirtualAddress());
        command_list->SetGraphicsRootDescriptorTable(
            root_param(EnumRootParamScene::BENCH_MATERIAL_TEXTURE),
            managers.shader.get_gpu_adr(eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_TEXTURE_BEGIN));
        command_list->SetGraphicsRootDescriptorTable(
            root_param(EnumRootParamScene::BENCH_MATERIAL_SAMPLER),
            resources_.sampler_heap->GetGPUDescriptorHandleForHeapStart());

        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetVertexBuffers(0, 1, &resources_.scene.vertex_buffer_view);
        command_list->IASetIndexBuffer(&resources_.scene.index_buffer_view);

        for (const auto& batch : resources_.scene.cpu->batches) {
            const auto& mesh = resources_.scene.cpu->meshes[batch.mesh_index];
            command_list->SetGraphicsRoot32BitConstant(
                root_param(EnumRootParamScene::DRAW_CONSTANT), batch.object_index, 0);
            command_list->DrawIndexedInstanced(
                mesh.index_count, batch.object_count, mesh.index_start, mesh.vertex_start, 0);
        }

        dxutl::transition_resource(command_list, resources_.back_buffers[frame_index],
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    }

}
