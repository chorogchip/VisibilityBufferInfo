#include "render/pass/benchmark/PassDebugView.h"

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceViewBuilder.h"
#include "engine/RootSignatureBuilder.h"
#include "util/Assertion.h"

namespace rndr {

    namespace {
        enum class RootParam : UINT {
            FRAME_CONSTANT,
            DRAW_CONSTANT,
            INSTANCE_BUFFER,
            MATERIAL_BUFFER,
        };
    }

    void PassDebugView::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDebugViewResources& resources) {

        resources_ = resources;

        resources_.frame_manager->create_rtv
        (eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0,
            resources_.back_buffers[0]->get());

        resources_.frame_manager->create_rtv(
            eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_1,
            resources_.back_buffers[1]->get());

        resources_.frame_manager->create_dsv(
            eng::ResourceManagerFrame::EnumDSV::DEPTH,
            resources_.depth->get());

        auto vs = dxutl::compile_shader(
            L"assets/shaders/debug_view_VS.hlsl",
            "vs_5_0", "main", arguments);
        auto ps = dxutl::compile_shader(
            L"assets/shaders/debug_view_PS.hlsl",
            "ps_5_0", "main", arguments);

        pso_.init(device);
        pso_.set_graphics();
        auto root_signature = eng::RootSignatureBuilder{}
            .set_flags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
            .root_cbv().reg(0).vis_vtx().add()         // FRAME_CONSTANT
            .constant().reg(1).cnt(1).vis_vtx().add()  // DRAW_CONSTANT
            .root_srv().reg(0).vis_vtx().add()         // INSTANCE_BUFFER
            .root_srv().reg(1).vis_pxl().add()         // MATERIAL_BUFFER
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_shader_pixel(ps.Get());
        pso_.build();
    }

    void PassDebugView::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {

        command_list->SetPipelineState(pso_.get());

        resources_.back_buffers[frame_index]->transition(command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);

        const auto rtv = resources_.frame_manager->get_rtv(
            eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0, frame_index);

        const auto dsv = resources_.frame_manager->get_dsv(
            eng::ResourceManagerFrame::EnumDSV::DEPTH);
        command_list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);

        constexpr float clear_color[] = { 0.1f, 0.1f, 0.15f, 1.f };
        command_list->ClearRenderTargetView(rtv, clear_color, 0, nullptr);
        command_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        command_list->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParam::FRAME_CONSTANT),
            resources_.constant_buffer_addresses[frame_index]);
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::INSTANCE_BUFFER),
            resources_.instance_buffer_address);
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::MATERIAL_BUFFER),
            resources_.material_buffer_address);

        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetVertexBuffers(0, 1, &resources_.vertex_buffer_view);
        command_list->IASetIndexBuffer(&resources_.index_buffer_view);

        for (const auto& batch : resources_.scene->batches) {
            const auto& mesh = resources_.scene->meshes[batch.mesh_index];
            command_list->SetGraphicsRoot32BitConstant(
                static_cast<UINT>(RootParam::DRAW_CONSTANT),
                batch.object_index, 0);
            command_list->DrawIndexedInstanced(
                mesh.index_count, batch.object_count, mesh.index_start, mesh.vertex_start, 0);
        }

    }

}
