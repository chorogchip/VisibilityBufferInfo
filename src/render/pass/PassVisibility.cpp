#include "render/pass/PassVisibility.h"

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "render/RootParameter.h"
#include "render/renderer/RendererBase.h"

namespace rndr {

    void PassVisibility::init(ID3D12Device* device, const util::ProgramArgument& arguments,
        const PassVisibilityResources& resources) {
        resources_ = resources;
        auto& frame = RendererBase::get_resource_manager().frame;
        frame.request_rtv(eng::ResourceManagerFrame::EnumRTV::BENCH_VISIBILITY, resources_.visibility);
        frame.request_dsv(eng::ResourceManagerFrame::EnumDSV::DEPTH, resources_.depth);
        RendererBase::get_resource_manager().shader.request(
            eng::ResourceManagerShader::EnumDescPos::BENCH_VISIBILITY_BUFFER,
            resources_.visibility);

        auto vs = dxutl::compile_shader(
            L"assets/shaders/visbuf_visibility_VS.hlsl", "vs_5_0", "main", arguments);
        auto ps = dxutl::compile_shader(
            L"assets/shaders/visbuf_visibility_PS.hlsl", "ps_5_0", "main", arguments);
        pso_.init(device);
        pso_.set_texture_count(arguments.texture_count);
        pso_.set_shaders(vs.Get(), ps.Get());
        pso_.set_render_targets(1, DXGI_FORMAT_R32G32_UINT);
        pso_.build();
    }

    void PassVisibility::render(ID3D12GraphicsCommandList* command_list, UINT frame_index,
        const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor_rect) {
        auto& frame = RendererBase::get_resource_manager().frame;
        dxutl::transition_resource(command_list, resources_.visibility,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

        command_list->SetPipelineState(pso_.get());
        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);
        command_list->SetGraphicsRootConstantBufferView(
            root_param(EnumRootParamScene::FRAME_CONSTANT),
            resources_.constant_buffers[frame_index]->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            root_param(EnumRootParamScene::BENCH_INSTANCE_BUFFER),
            resources_.scene.instance_buffer->GetGPUVirtualAddress());
        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetVertexBuffers(0, 1, &resources_.scene.vertex_buffer_view);
        command_list->IASetIndexBuffer(&resources_.scene.index_buffer_view);

        const auto rtv = frame.get_rtv(eng::ResourceManagerFrame::EnumRTV::BENCH_VISIBILITY);
        const auto dsv = frame.get_dsv(eng::ResourceManagerFrame::EnumDSV::DEPTH);
        command_list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        constexpr float clear[] = { 0.f, 0.f, 0.f, 0.f };
        command_list->ClearRenderTargetView(rtv, clear, 0, nullptr);
        command_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        for (const auto& batch : resources_.scene.cpu->batches) {
            const auto& mesh = resources_.scene.cpu->meshes[batch.mesh_index];
            command_list->SetGraphicsRoot32BitConstant(
                root_param(EnumRootParamScene::DRAW_CONSTANT), batch.object_index, 0);
            command_list->DrawIndexedInstanced(mesh.index_count, batch.object_count,
                mesh.index_start, mesh.vertex_start, 0);
        }
    }

}
