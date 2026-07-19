#include "render/pass/PassDepthPre.h"

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerShader.h"
#include "render/RootParameter.h"

namespace rndr {

    void PassDepthPre::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDepthPreResources& resources) {
        resources_ = resources;
        resources_.frame_manager->create_dsv(eng::ResourceManagerFrame::EnumDSV::DEPTH, resources_.depth);

        auto vertex_shader = dxutl::compile_shader(
            L"assets/shaders/depth_prepass_VS.hlsl", "vs_5_0", "main", arguments);

        pso_.init(device);
        pso_.set_texture_count(arguments.texture_count);
        pso_.set_shaders(vertex_shader.Get(), nullptr);
        pso_.set_depth_only();
        pso_.build();
    }

    void PassDepthPre::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {
        dxutl::transition_resource(command_list, resources_.depth,
            D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

        command_list->SetPipelineState(pso_.get());
        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);
        command_list->SetGraphicsRootConstantBufferView(
            root_param(EnumRootParamScene::FRAME_CONSTANT),
            resources_.constant_buffers[frame_index]->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            root_param(EnumRootParamScene::BENCH_INSTANCE_BUFFER),
            resources_.instance_buffer->GetGPUVirtualAddress());

        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetVertexBuffers(0, 1, &resources_.vertex_buffer_view);
        command_list->IASetIndexBuffer(&resources_.index_buffer_view);

        const auto dsv = resources_.frame_manager->get_dsv(eng::ResourceManagerFrame::EnumDSV::DEPTH);
        command_list->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
        command_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        for (const auto& batch : resources_.scene->batches) {
            const auto& mesh = resources_.scene->meshes[batch.mesh_index];
            command_list->SetGraphicsRoot32BitConstant(
                root_param(EnumRootParamScene::DRAW_CONSTANT), batch.object_index, 0);
            command_list->DrawIndexedInstanced(
                mesh.index_count, batch.object_count, mesh.index_start, mesh.vertex_start, 0);
        }
    }

}
