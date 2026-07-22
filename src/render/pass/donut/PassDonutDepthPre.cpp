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

        D3D12_SHADER_RESOURCE_VIEW_DESC make_structured_srv_desc(
            UINT element_count,
            UINT element_stride) {

            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = element_count;
            desc.Buffer.StructureByteStride = element_stride;
            desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            return desc;
        }

        void validate_resources(const PassDonutDepthPreResources& resources) {
            util::Logger::g_logger.assert_with_log(
                resources.frame_manager != nullptr &&
                resources.shader_manager != nullptr &&
                resources.depth != nullptr &&
                resources.scene != nullptr,
                "Donut depth pre-pass requires valid resources");
            util::Logger::g_logger.assert_with_log(
                resources.scene->vertex_buffer != nullptr &&
                resources.scene->index_buffer != nullptr &&
                resources.scene->instance_buffer != nullptr,
                "Donut depth pre-pass requires scene buffers");
            for (UINT frame_index = 0;
                frame_index < util::Constants::FRAME_COUNT;
                ++frame_index) {
                util::Logger::g_logger.assert_with_log(
                    resources.constant_buffers[frame_index] != nullptr,
                    "Donut depth pre-pass requires frame constant buffers");
            }
        }
    }

    void PassDonutDepthPre::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutDepthPreResources& resources,
        bool use_prepass_depth) {

        (void)use_prepass_depth;

        resources_ = resources;
        validate_resources(resources_);

        resources_.frame_manager->create_dsv(
            eng::ResourceManagerFrame::EnumDSV::DEPTH,
            resources_.depth->get());

        const D3D12_SHADER_RESOURCE_VIEW_DESC instance_srv =
            make_structured_srv_desc(
                static_cast<UINT>(resources_.scene->instance_data.size()),
                sizeof(scene::DonutSceneDataGPU::InstanceData));

        resources_.shader_manager->create_srv(
            resources_.scene->instance_buffer.Get(),
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
                draw.instance_id,
                0,
                resources_.scene->vertex_layout.position_offset,
                resources_.scene->vertex_layout.texcoord_offset
            };
            command_list->SetGraphicsRoot32BitConstants(
                static_cast<UINT>(RootParam::PUSH_CONSTANT),
                PUSH_CONSTANT_DWORD_COUNT, &push_constants, 0);
            command_list->DrawIndexedInstanced(
                draw.index_count, 1, draw.index_offset, 0, 0);
        }
    }
}
