#include "render/pass/donut/PassDonutDepthPre.h"

#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerShader.h"
#include "engine/ResourceViewBuilder.h"
#include "engine/RootSignatureBuilder.h"
#include "util/Assertion.h"
#include "util/Logger.h"

namespace rndr {

    namespace {

        enum class RootParam : UINT {
            PUSH_CONSTANT,
            VIEW_CONSTANT,
            INSTANCE_BUFFER,
            VERTEX_BUFFER,
        };

        struct PushConstants {
            uint32_t start_instance_location = 0;
            uint32_t start_vertex_location = 0;
            uint32_t position_offset = 0;
            uint32_t texcoord_offset = 0;
        };

        static constexpr UINT PUSH_CONSTANT_DWORD_COUNT =
            sizeof(PushConstants) / sizeof(uint32_t);
    }

    void PassDonutDepthPre::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutDepthPreResources& resources) {

        resources_ = resources;

        resources_.frame_manager->create_dsv(
            eng::ResourceManagerFrame::EnumDSV::DEPTH,
            resources_.depth->get());

        D3D12_SHADER_RESOURCE_VIEW_DESC instance_srv{};
        instance_srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        instance_srv.Format = DXGI_FORMAT_UNKNOWN;
        instance_srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        instance_srv.Buffer.FirstElement = 0;
        instance_srv.Buffer.NumElements = static_cast<UINT>(
            resources_.scene->render_instance_data.size());
        instance_srv.Buffer.StructureByteStride = sizeof(scene::DonutSceneDataGPU::InstanceData);
        instance_srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        resources_.shader_manager->create_srv(
            resources_.scene->render_instance_buffer.Get(),
            instance_srv,
            eng::ResourceManagerShader::EnumDescPos::DONUT_INSTANCE_BUFFER);
        resources_.shader_manager->create_srv(
            resources_.scene->vertex_buffer.Get(),
            eng::ResourceViewBuilder::build_srv(resources_.scene->vertex_buffer.Get()),
            eng::ResourceManagerShader::EnumDescPos::DONUT_VERTEX_BUFFER);

        util::assure_next<
            eng::ResourceManagerShader::EnumDescPos::DONUT_INSTANCE_BUFFER,
            eng::ResourceManagerShader::EnumDescPos::DONUT_VERTEX_BUFFER>();

        auto vs = dxutl::compile_shader(
            L"assets/shaders/donut/donut_depth_VS.hlsl",
            "vs_5_1", "buffer_loads", arguments);

        pso_.init(device);
        pso_.set_graphics();
        auto root_signature = eng::RootSignatureBuilder{}
            .set_flags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
            .constant().reg(1).cnt(PUSH_CONSTANT_DWORD_COUNT).spc(1).vis_vtx().add()
            .root_cbv().reg(2).spc(2).vis_vtx().add()
            .srv_tabl().reg(10).cnt(1).spc(1).vis_vtx().add()
            .srv_tabl().reg(11).cnt(1).spc(1).vis_vtx().add()
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_manual_vertex_fetch();
        pso_.set_depth_only();
        pso_.build();
    }

    void PassDonutDepthPre::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {

        resources_.depth->transition(command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);

        command_list->SetPipelineState(pso_.get());
        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        ID3D12DescriptorHeap* heaps[] = { resources_.shader_manager->get() };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);

        command_list->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParam::VIEW_CONSTANT),
            resources_.constant_buffers[frame_index]->get()->GetGPUVirtualAddress());
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::INSTANCE_BUFFER),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_INSTANCE_BUFFER));
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::VERTEX_BUFFER),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_VERTEX_BUFFER));

        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetIndexBuffer(&resources_.scene->index_buffer_view);

        const auto dsv = resources_.frame_manager->get_dsv(
            eng::ResourceManagerFrame::EnumDSV::DEPTH);
        command_list->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
        command_list->ClearDepthStencilView(
            dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        for (const scene::DonutSceneDataGPU::Draw& draw : resources_.scene->draws) {
            const PushConstants push_constants{
                draw.first_render_instance,
                0,
                resources_.scene->vertex_layout.position_offset,
                resources_.scene->vertex_layout.texcoord_offset
            };
            command_list->SetGraphicsRoot32BitConstants(
                static_cast<UINT>(RootParam::PUSH_CONSTANT),
                PUSH_CONSTANT_DWORD_COUNT, &push_constants, 0);
            command_list->DrawIndexedInstanced(
                draw.index_count, draw.instance_count, draw.index_offset, 0, 0);
        }
    }
}
