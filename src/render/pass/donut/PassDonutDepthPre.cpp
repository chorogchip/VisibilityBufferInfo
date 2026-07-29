#include "render/pass/donut/PassDonutDepthPre.h"

#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
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
            DRAW_INSTANCE_BUFFER,
            DRAW_INSTANCE_ID_BUFFER,
            MATERIAL_CONSTANT,
            MATERIAL_TEXTURES,
            MATERIAL_SAMPLER,
        };

        enum class JunglePointRootParam : UINT {
            PUSH_CONSTANT,
            VIEW_CONSTANT,
            VERTEX_BUFFER,
            POINT_INSTANCE_BUFFER,
            POINT_PROTOTYPE_BUFFER,
            POINT_INSTANCE_ID_BUFFER,
            MATERIAL_CONSTANT,
            MATERIAL_TEXTURES,
            MATERIAL_SAMPLER,
        };

        struct PushConstants {
            uint32_t start_instance_location = 0;
            uint32_t start_vertex_location = 0;
            uint32_t position_offset = 0;
            uint32_t texcoord_offset = 0;
        };

        struct JunglePointPushConstants {
            uint32_t start_instance_location = 0;
            uint32_t prototype_id = 0;
            uint32_t position_offset = 0;
            uint32_t texcoord_offset = 0;
        };

        static constexpr UINT PUSH_CONSTANT_DWORD_COUNT =
            sizeof(PushConstants) / sizeof(uint32_t);
        static constexpr UINT
            JUNGLE_POINT_PUSH_CONSTANT_DWORD_COUNT =
                sizeof(JunglePointPushConstants) /
                sizeof(uint32_t);
    }

    void PassDonutDepthPre::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutDepthPreResources& resources) {

        resources_ = resources;
        util::Logger::g_logger.assert_with_log(
            (resources_.jungle_scene == nullptr) ==
                (resources_.jungle_draw_stream == nullptr),
            "Donut depth Jungle point resources are incomplete.");

        resources_.frame_manager->create_dsv(
            eng::ResourceManagerFrame::EnumDSV::DEPTH,
            resources_.depth->get());

        for (UINT material_id = 0;
            material_id < resources_.scene->material_data.size();
            ++material_id) {
            const scene::DonutSceneGPUData::MaterialData& material =
                resources_.scene->material_data[material_id];
            for (UINT slot_index = 0;
                slot_index < MATERIAL_TEXTURE_DESCRIPTOR_COUNT;
                ++slot_index) {
                const uint32_t texture_index = material.texture_indices[slot_index];
                util::Logger::g_logger.assert_with_log(
                    texture_index < resources_.scene->textures.size(),
                    "Donut depth material texture index is invalid");

                resources_.shader_manager->create_srv(
                    resources_.scene->textures[texture_index].get(),
                    eng::ResourceViewBuilder::build_srv(
                        resources_.scene->textures[texture_index].get(),
                        eng::ResourceViewBuilder::EnumResourceType::TEXTURE_2D),
                    eng::ResourceManagerShader::EnumDescPos::DONUT_MATERIAL_TEXTURE_BEGIN,
                    material_id * MATERIAL_TEXTURE_DESCRIPTOR_COUNT + slot_index);
            }
        }

        resources_.sampler_manager->create_sampler(
            eng::ResourceManagerSampler::EnumDescPos::DONUT_MATERIAL,
            eng::ResourceManagerSampler::EnumSamplerType::LINEAR_WRAP);

        auto vs = dxutl::compile_shader(
            L"assets/shaders/mydonut/donut_depth_pre_VS.hlsl",
            L"vs_6_5", L"buffer_loads", arguments);
        auto ps = dxutl::compile_shader(
            L"assets/shaders/mydonut/donut_depth_pre_PS.hlsl",
            L"ps_6_5", L"main", arguments);

        pso_.init(device);
        pso_.set_graphics();
        auto root_signature = eng::RootSignatureBuilder{}
            .set_flags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
            .constant().reg(1).cnt(PUSH_CONSTANT_DWORD_COUNT).spc(1).vis_vtx().add()
            .root_cbv().reg(2).spc(2).vis_vtx().add()
            .root_srv().reg(10).spc(1).vis_vtx().add()
            .root_srv().reg(11).spc(1).vis_vtx().add()
            .root_srv().reg(12).spc(1).vis_vtx().add()
            .root_srv().reg(13).spc(1).vis_vtx().add()
            .root_cbv().reg(0).spc(0).vis_pxl().add()
            .srv_tabl().reg(0).cnt(MATERIAL_TEXTURE_DESCRIPTOR_COUNT).spc(0).vis_pxl().add()
            .spl_tabl().reg(0).cnt(1).spc(2).vis_pxl().add()
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_shader_pixel(ps.Get());
        pso_.set_manual_vertex_fetch();
        pso_.set_depth_only();
        pso_.build();

        if (resources_.jungle_scene != nullptr) {
            auto jungle_vs = dxutl::compile_shader(
                L"assets/shaders/mydonut/jungle_point_depth_pre_VS.hlsl",
                L"vs_6_5", L"main", arguments);
            jungle_point_pso_.init(device);
            jungle_point_pso_.set_graphics();
            auto jungle_root_signature =
                eng::RootSignatureBuilder{}
                    .set_flags(
                        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
                    .constant().reg(1)
                        .cnt(JUNGLE_POINT_PUSH_CONSTANT_DWORD_COUNT)
                        .spc(1).vis_vtx().add()
                    .root_cbv().reg(2).spc(2).vis_vtx().add()
                    .root_srv().reg(11).spc(1).vis_vtx().add()
                    .root_srv().reg(14).spc(1).vis_vtx().add()
                    .root_srv().reg(15).spc(1).vis_vtx().add()
                    .root_srv().reg(16).spc(1).vis_vtx().add()
                    .root_cbv().reg(0).spc(0).vis_pxl().add()
                    .srv_tabl().reg(0)
                        .cnt(MATERIAL_TEXTURE_DESCRIPTOR_COUNT)
                        .spc(0).vis_pxl().add()
                    .spl_tabl().reg(0).cnt(1)
                        .spc(2).vis_pxl().add()
                    .build(device);
            jungle_point_pso_.set_root_signature(
                jungle_root_signature.Get());
            jungle_point_pso_.set_shader_vertex(jungle_vs.Get());
            jungle_point_pso_.set_shader_pixel(ps.Get());
            jungle_point_pso_.set_manual_vertex_fetch();
            jungle_point_pso_.set_depth_only();
            jungle_point_pso_.build();
        }
    }

    void PassDonutDepthPre::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {

        resources_.depth->transition(command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);

        command_list->SetPipelineState(pso_.get());
        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        ID3D12DescriptorHeap* heaps[] = {
            resources_.shader_manager->get(),
            resources_.sampler_manager->get() };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);

        command_list->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParam::VIEW_CONSTANT),
            resources_.constant_buffers[frame_index]->get()->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::INSTANCE_BUFFER),
            resources_.scene->instance_buffer.get()->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::VERTEX_BUFFER),
            resources_.scene->vertex_buffer.get()->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::DRAW_INSTANCE_BUFFER),
            resources_.scene->draw_instance_buffer.get()->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::DRAW_INSTANCE_ID_BUFFER),
            resources_.scene->draw_instance_id_buffer.get()->GetGPUVirtualAddress());
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::MATERIAL_SAMPLER),
            resources_.sampler_manager->get_gpu_adr(
                eng::ResourceManagerSampler::EnumDescPos::DONUT_MATERIAL));

        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetIndexBuffer(&resources_.scene->index_buffer_view);

        const auto dsv = resources_.frame_manager->get_dsv(
            eng::ResourceManagerFrame::EnumDSV::DEPTH);
        command_list->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
        command_list->ClearDepthStencilView(
            dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        for (const auto& draw :
            resources_.draw_stream->draw_calls_compacted) {
            const PushConstants push_constants{
                draw.first_instance,
                0,
                resources_.scene->vertex_layout.position_offset,
                resources_.scene->vertex_layout.texcoord_offset
            };
            command_list->SetGraphicsRoot32BitConstants(
                static_cast<UINT>(RootParam::PUSH_CONSTANT),
                PUSH_CONSTANT_DWORD_COUNT, &push_constants, 0);

            const D3D12_GPU_VIRTUAL_ADDRESS material_address =
                resources_.scene->material_constant_buffer.get()->
                    GetGPUVirtualAddress() +
                static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(draw.material_id) *
                resources_.scene->material_constant_stride;
            command_list->SetGraphicsRootConstantBufferView(
                static_cast<UINT>(RootParam::MATERIAL_CONSTANT),
                material_address);
            command_list->SetGraphicsRootDescriptorTable(
                static_cast<UINT>(RootParam::MATERIAL_TEXTURES),
                resources_.shader_manager->get_gpu_adr(
                    eng::ResourceManagerShader::EnumDescPos::DONUT_MATERIAL_TEXTURE_BEGIN,
                    draw.material_id * MATERIAL_TEXTURE_DESCRIPTOR_COUNT));

            command_list->DrawIndexedInstanced(
                draw.index_count, draw.instance_count, draw.index_offset, 0, 0);
        }

        if (resources_.jungle_scene == nullptr) {
            return;
        }

        command_list->SetPipelineState(
            jungle_point_pso_.get());
        command_list->SetGraphicsRootSignature(
            jungle_point_pso_.get_root_signature());
        command_list->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(
                JunglePointRootParam::VIEW_CONSTANT),
            resources_.constant_buffers[frame_index]->get()->
                GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(
                JunglePointRootParam::VERTEX_BUFFER),
            resources_.scene->vertex_buffer.get()->
                GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(
                JunglePointRootParam::POINT_INSTANCE_BUFFER),
            resources_.jungle_scene->point_instance_buffer.get()->
                GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(
                JunglePointRootParam::POINT_PROTOTYPE_BUFFER),
            resources_.jungle_scene->point_prototype_buffer.get()->
                GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(
                JunglePointRootParam::POINT_INSTANCE_ID_BUFFER),
            resources_.jungle_scene->point_instance_id_buffer.get()->
                GetGPUVirtualAddress());
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(
                JunglePointRootParam::MATERIAL_SAMPLER),
            resources_.sampler_manager->get_gpu_adr(
                eng::ResourceManagerSampler::EnumDescPos::
                    DONUT_MATERIAL));

        for (const auto& draw :
            resources_.jungle_draw_stream->
                point_draw_calls_compacted) {
            const JunglePointPushConstants push_constants{
                draw.first_instance,
                draw.prototype_id,
                resources_.scene->vertex_layout.position_offset,
                resources_.scene->vertex_layout.texcoord_offset
            };
            command_list->SetGraphicsRoot32BitConstants(
                static_cast<UINT>(
                    JunglePointRootParam::PUSH_CONSTANT),
                JUNGLE_POINT_PUSH_CONSTANT_DWORD_COUNT,
                &push_constants,
                0);

            const D3D12_GPU_VIRTUAL_ADDRESS material_address =
                resources_.scene->material_constant_buffer.get()->
                    GetGPUVirtualAddress() +
                static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(
                    draw.material_id) *
                resources_.scene->material_constant_stride;
            command_list->SetGraphicsRootConstantBufferView(
                static_cast<UINT>(
                    JunglePointRootParam::MATERIAL_CONSTANT),
                material_address);
            command_list->SetGraphicsRootDescriptorTable(
                static_cast<UINT>(
                    JunglePointRootParam::MATERIAL_TEXTURES),
                resources_.shader_manager->get_gpu_adr(
                    eng::ResourceManagerShader::EnumDescPos::
                        DONUT_MATERIAL_TEXTURE_BEGIN,
                    draw.material_id *
                        MATERIAL_TEXTURE_DESCRIPTOR_COUNT));

            command_list->DrawIndexedInstanced(
                draw.index_count,
                draw.instance_count,
                draw.index_offset,
                0,
                0);
        }
    }
}
