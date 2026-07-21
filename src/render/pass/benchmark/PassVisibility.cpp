#include "render/pass/benchmark/PassVisibility.h"

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/RootSignatureBuilder.h"
#include "util/Assertion.h"

namespace rndr {

    namespace {
        enum class RootParam : UINT {
            FRAME_CONSTANT,
            DRAW_CONSTANT,
            INSTANCE_BUFFER,
        };
    }

    void PassVisibility::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassVisibilityResources& resources) {

        resources_ = resources;

        resources_.frame_manager->create_rtv(
            eng::ResourceManagerFrame::EnumRTV::BENCH_VISIBILITY,
            resources_.visibility->get());
        resources_.frame_manager->create_dsv(
            eng::ResourceManagerFrame::EnumDSV::DEPTH,
            resources_.depth->get());

        auto vs = dxutl::compile_shader(
            L"assets/shaders/visbuf_visibility_VS.hlsl",
            "vs_5_0", "main", arguments);
        auto ps = dxutl::compile_shader(
            L"assets/shaders/visbuf_visibility_PS.hlsl",
            "ps_5_0", "main", arguments);

        pso_.init(device);
        pso_.set_graphics();
        auto root_signature = eng::RootSignatureBuilder{}
            .set_flags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
            .root_cbv().reg(0).vis(D3D12_SHADER_VISIBILITY_VERTEX).add()         // FRAME_CONSTANT
            .constant().reg(1).cnt(1).vis(D3D12_SHADER_VISIBILITY_VERTEX).add()  // DRAW_CONSTANT
            .root_srv().reg(0).vis(D3D12_SHADER_VISIBILITY_VERTEX).add()         // INSTANCE_BUFFER
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_shader_pixel(ps.Get());
        pso_.set_render_targets(1, DXGI_FORMAT_R32G32_UINT);
        pso_.build();
    }

    void PassVisibility::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {

        resources_.visibility->transition(
            command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
        command_list->SetPipelineState(pso_.get());
        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);
        command_list->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParam::FRAME_CONSTANT),
            resources_.constant_buffers[frame_index]->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::INSTANCE_BUFFER),
            resources_.instance_buffer->GetGPUVirtualAddress());
        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetVertexBuffers(0, 1, &resources_.vertex_buffer_view);
        command_list->IASetIndexBuffer(&resources_.index_buffer_view);

        const auto rtv = resources_.frame_manager->get_rtv(
            eng::ResourceManagerFrame::EnumRTV::BENCH_VISIBILITY);
        const auto dsv = resources_.frame_manager->get_dsv(
            eng::ResourceManagerFrame::EnumDSV::DEPTH);
        command_list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        constexpr float clear[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        command_list->ClearRenderTargetView(rtv, clear, 0, nullptr);
        command_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        for (const auto& batch : resources_.scene->batches) {
            const auto& mesh = resources_.scene->meshes[batch.mesh_index];
            command_list->SetGraphicsRoot32BitConstant(
                static_cast<UINT>(RootParam::DRAW_CONSTANT),
                batch.object_index, 0);
            command_list->DrawIndexedInstanced(mesh.index_count, batch.object_count,
                mesh.index_start, mesh.vertex_start, 0);
        }
    }

}
