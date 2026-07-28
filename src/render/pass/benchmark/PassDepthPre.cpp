#include "render/pass/benchmark/PassDepthPre.h"

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerShader.h"
#include "engine/RootSignatureBuilder.h"
#include "util/Assertion.h"


namespace rndr {

    namespace {
        enum class RootParam : UINT {
            FRAME_CONSTANT,
            DRAW_CONSTANT,
            INSTANCE_BUFFER,
            DRAW_INSTANCE_BUFFER,
            DRAW_INSTANCE_ID_BUFFER,
        };

        struct DrawConstants {
            uint32_t first_instance = 0;
            uint32_t material_id = 0;
        };
    }

    void PassDepthPre::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDepthPreResources& resources) {

        resources_ = resources;

        resources_.frame_manager->create_dsv(
            eng::ResourceManagerFrame::EnumDSV::DEPTH,
            resources_.depth->get());

        auto vs = dxutl::compile_shader(
            L"assets/shaders/benchmark/depth_prepass_VS.hlsl",
            L"vs_6_5", L"main", arguments);

        pso_.init(device);
        pso_.set_graphics();
        auto root_signature = eng::RootSignatureBuilder{}
            .set_flags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
            .root_cbv().reg(0).vis_vtx().add()         // FRAME_CONSTANT
            .constant().reg(1).cnt(2).vis_vtx().add()  // DRAW_CONSTANT
            .root_srv().reg(0).vis_vtx().add()         // INSTANCE_BUFFER
            .root_srv().reg(10).vis_vtx().add()        // DRAW_INSTANCE_BUFFER
            .root_srv().reg(11).vis_vtx().add()        // DRAW_INSTANCE_ID_BUFFER
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_depth_only();
        pso_.build();
    }

    void PassDepthPre::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {

        resources_.depth->transition(command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);

        command_list->SetPipelineState(pso_.get());
        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);
        command_list->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParam::FRAME_CONSTANT),
            resources_.constant_buffer_addresses[frame_index]);
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::INSTANCE_BUFFER),
            resources_.instance_buffer_address);
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::DRAW_INSTANCE_BUFFER),
            resources_.draw_instance_buffer_address);
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::DRAW_INSTANCE_ID_BUFFER),
            resources_.draw_instance_id_buffer_address);

        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetVertexBuffers(0, 1, &resources_.vertex_buffer_view);
        command_list->IASetIndexBuffer(&resources_.index_buffer_view);

        const auto dsv = resources_.frame_manager->get_dsv(eng::ResourceManagerFrame::EnumDSV::DEPTH);
        command_list->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
        command_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        for (const auto& draw : resources_.draw_stream->draw_calls_compacted) {
            const DrawConstants draw_constants{
                draw.first_instance,
                draw.material_id
            };
            command_list->SetGraphicsRoot32BitConstants(
                static_cast<UINT>(RootParam::DRAW_CONSTANT),
                2, &draw_constants, 0);
            command_list->DrawIndexedInstanced(
                draw.index_count,
                draw.instance_count,
                draw.index_offset,
                draw.vertex_offset,
                0);
        }
    }

}
