#include "render/pass/donut/PassDonutVisibility.h"

#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"
#include "engine/ResourceViewBuilder.h"
#include "engine/RootSignatureBuilder.h"
#include "util/Logger.h"

namespace rndr {

    namespace {
        enum class RootParam : UINT {
            PUSH_CONSTANT,
            VIEW_CONSTANT,
            INSTANCE_BUFFER,
            VERTEX_BUFFER,
            MATERIAL_CONSTANT,
            MATERIAL_TEXTURES,
            MATERIAL_SAMPLER,
        };

        struct PushConstants {
            uint32_t start_instance_location = 0;
            uint32_t submesh_id = 0;
            uint32_t position_offset = 0;
            uint32_t texcoord_offset = 0;
        };

        static constexpr UINT PUSH_CONSTANT_DWORD_COUNT =
            sizeof(PushConstants) / sizeof(uint32_t);
    }

    void PassDonutVisibility::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutVisibilityResources& resources,
        bool use_prepass_depth) {

        resources_ = resources;
        use_prepass_depth_ = use_prepass_depth;

        resources_.frame_manager->create_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_VISIBILITY,
            resources_.visibility_buf->get());
        resources_.frame_manager->create_dsv(
            use_prepass_depth_ ?
            eng::ResourceManagerFrame::EnumDSV::DEPTH_READ_ONLY :
            eng::ResourceManagerFrame::EnumDSV::DEPTH,
            resources_.depth->get());

        for (UINT material_id = 0;
            material_id < resources_.scene->material_data.size();
            ++material_id) {
            const scene::DonutSceneDataGPU::MaterialData& material =
                resources_.scene->material_data[material_id];
            for (UINT slot_index = 0;
                slot_index < MATERIAL_TEXTURE_DESCRIPTOR_COUNT;
                ++slot_index) {
                const uint32_t texture_index = material.texture_indices[slot_index];
                util::Logger::g_logger.assert_with_log(
                    texture_index < resources_.scene->textures.size(),
                    "Donut visibility material texture index is invalid");

                resources_.shader_manager->create_srv(
                    resources_.scene->textures[texture_index].Get(),
                    eng::ResourceViewBuilder::build_srv(
                        resources_.scene->textures[texture_index].Get(),
                        eng::ResourceViewBuilder::EnumResourceType::TEXTURE_2D),
                    eng::ResourceManagerShader::EnumDescPos::DONUT_MATERIAL_TEXTURE_BEGIN,
                    material_id * MATERIAL_TEXTURE_DESCRIPTOR_COUNT + slot_index);
            }
        }

        resources_.sampler_manager->create_sampler(
            eng::ResourceManagerSampler::EnumDescPos::DONUT_MATERIAL,
            eng::ResourceManagerSampler::EnumSamplerType::LINEAR_WRAP);

        auto vs = dxutl::compile_shader(
            L"assets/shaders/donut_visibility_VS.hlsl",
            "vs_5_1", "main", arguments);
        auto ps = dxutl::compile_shader(
            L"assets/shaders/donut_visibility_PS.hlsl",
            "ps_5_1", "main", arguments);

        pso_.init(device);
        pso_.set_graphics();
        auto root_signature = eng::RootSignatureBuilder{}
            .set_flags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
            .constant().reg(1).cnt(PUSH_CONSTANT_DWORD_COUNT).spc(1).vis_vtx().add()
            .root_cbv().reg(2).spc(2).vis_vtx().add()
            .root_srv().reg(10).spc(1).vis_vtx().add()
            .root_srv().reg(11).spc(1).vis_vtx().add()
            .root_cbv().reg(0).spc(0).vis_pxl().add()
            .srv_tabl().reg(0).cnt(MATERIAL_TEXTURE_DESCRIPTOR_COUNT).spc(0).vis_pxl().add()
            .spl_tabl().reg(0).cnt(1).spc(2).vis_pxl().add()
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_shader_pixel(ps.Get());
        pso_.set_manual_vertex_fetch();
        pso_.set_render_targets(1, DXGI_FORMAT_R32G32_UINT);
        if (use_prepass_depth_)
            pso_.set_depth_equal();
        pso_.build();
    }

    void PassDonutVisibility::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {

        resources_.visibility_buf->transition(
            command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
        resources_.depth->transition(
            command_list,
            use_prepass_depth_ ?
            D3D12_RESOURCE_STATE_DEPTH_READ :
            D3D12_RESOURCE_STATE_DEPTH_WRITE);

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
            resources_.scene->render_instance_buffer->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::VERTEX_BUFFER),
            resources_.scene->vertex_buffer->GetGPUVirtualAddress());
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::MATERIAL_SAMPLER),
            resources_.sampler_manager->get_gpu_adr(
                eng::ResourceManagerSampler::EnumDescPos::DONUT_MATERIAL));

        const auto rtv = resources_.frame_manager->get_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_VISIBILITY);
        const auto dsv = resources_.frame_manager->get_dsv(
            use_prepass_depth_ ?
            eng::ResourceManagerFrame::EnumDSV::DEPTH_READ_ONLY :
            eng::ResourceManagerFrame::EnumDSV::DEPTH);
        command_list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

        constexpr float clear_visibility[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        command_list->ClearRenderTargetView(rtv, clear_visibility, 0, nullptr);
        if (!use_prepass_depth_) {
            command_list->ClearDepthStencilView(
                dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
        }

        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetIndexBuffer(&resources_.scene->index_buffer_view);

        for (const scene::DonutSceneDataGPU::Draw& draw : resources_.scene->draws) {
            const PushConstants push_constants{
                draw.first_render_instance,
                draw.submesh_id,
                resources_.scene->vertex_layout.position_offset,
                resources_.scene->vertex_layout.texcoord_offset
            };
            command_list->SetGraphicsRoot32BitConstants(
                static_cast<UINT>(RootParam::PUSH_CONSTANT),
                PUSH_CONSTANT_DWORD_COUNT, &push_constants, 0);

            const D3D12_GPU_VIRTUAL_ADDRESS material_address =
                resources_.scene->material_constant_buffer->GetGPUVirtualAddress() +
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
    }
}
