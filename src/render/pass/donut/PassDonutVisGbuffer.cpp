#include "render/pass/donut/PassDonutVisGbuffer.h"

#include <string>

#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"
#include "engine/ResourceViewBuilder.h"
#include "engine/RootSignatureBuilder.h"
#include "util/Logger.h"
#include "util/RenderConstants.h"

namespace rndr {

    namespace {
        enum class RootParam : UINT {
            VERTEX_LAYOUT,
            VIEW_CONSTANT,
            VISIBILITY,
            INDEX_BUFFER,
            VERTEX_BUFFER,
            INSTANCE_BUFFER,
            SUBMESH_BUFFER,
            GEOMETRY_INSTANCE_BUFFER,
            MATERIAL_BUFFER,
            MATERIAL_TEXTURES,
            MATERIAL_SAMPLER,
        };

        struct VertexLayoutConstants {
            uint32_t position_offset = 0;
            uint32_t texcoord_offset = 0;
            uint32_t normal_offset = 0;
            uint32_t tangent_offset = 0;
        };

        static constexpr UINT VERTEX_LAYOUT_DWORD_COUNT =
            sizeof(VertexLayoutConstants) / sizeof(uint32_t);
    }

    void PassDonutVisGBuffer::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutVisGBufferResources& resources) {

        resources_ = resources;
        use_motion_vectors_ = false;

        resources_.shader_manager->create_srv(
            resources_.visibility->get(),
            eng::ResourceViewBuilder::build_srv(
                resources_.visibility->get(),
                eng::ResourceViewBuilder::EnumResourceType::TEXTURE_2D,
                DXGI_FORMAT_R32G32_UINT),
            eng::ResourceManagerShader::EnumDescPos::DONUT_VISIBILITY_BUFFER);

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
                    "Donut visibility G-buffer material texture index is invalid");

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

        resources_.frame_manager->create_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_0,
            resources_.gbuffers[0]->get());
        resources_.frame_manager->create_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_1,
            resources_.gbuffers[1]->get());
        resources_.frame_manager->create_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_2,
            resources_.gbuffers[2]->get());
        resources_.frame_manager->create_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_3,
            resources_.gbuffers[3]->get());

        const UINT material_texture_descriptor_count =
            static_cast<UINT>(resources_.scene->material_data.size()) *
            MATERIAL_TEXTURE_DESCRIPTOR_COUNT;
        util::Logger::g_logger.assert_with_log(
            material_texture_descriptor_count <=
            scene::DonutSceneDataGPU::MAX_MATERIAL_TEXTURE_DESCRIPTOR_COUNT,
            "Donut visibility G-buffer material texture descriptor count exceeds shader limit");
        const std::string material_descriptor_count_define =
            std::to_string(material_texture_descriptor_count);
        const D3D_SHADER_MACRO ps_defines[] = {
            { "DONUT_MATERIAL_TEXTURE_DESCRIPTOR_COUNT",
                material_descriptor_count_define.c_str() },
            { nullptr, nullptr }
        };

        auto vs = dxutl::compile_shader(
            L"assets/shaders/visbuf_lighting_VS.hlsl",
            "vs_5_1", "main", arguments);
        auto ps = dxutl::compile_shader(
            L"assets/shaders/donut_vis_gbuffer_PS.hlsl",
            "ps_5_1", "main", ps_defines);

        pso_.init(device);
        pso_.set_graphics();
        auto root_signature = eng::RootSignatureBuilder{}
            .constant().reg(1).cnt(VERTEX_LAYOUT_DWORD_COUNT).spc(1).vis_pxl().add()
            .root_cbv().reg(2).spc(2).vis_pxl().add()
            .srv_tabl().reg(20).cnt(1).spc(1).vis_pxl().add()
            .root_srv().reg(21).spc(1).vis_pxl().add()
            .root_srv().reg(22).spc(1).vis_pxl().add()
            .root_srv().reg(23).spc(1).vis_pxl().add()
            .root_srv().reg(24).spc(1).vis_pxl().add()
            .root_srv().reg(25).spc(1).vis_pxl().add()
            .root_srv().reg(26).spc(1).vis_pxl().add()
            .srv_tabl().reg(0).cnt(material_texture_descriptor_count).spc(0).vis_pxl().add()
            .spl_tabl().reg(0).cnt(1).spc(2).vis_pxl().add()
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_shader_pixel(ps.Get());
        pso_.set_fullscreen();
        pso_.set_render_targets(
            static_cast<UINT>(util::RenderConstants::DONUT_GBUFFER_FORMATS.size()),
            util::RenderConstants::DONUT_GBUFFER_FORMATS.data());
        pso_.build();
    }

    void PassDonutVisGBuffer::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {

        resources_.visibility->transition(
            command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        for (eng::GPUResource* gbuffer : resources_.gbuffers) {
            gbuffer->transition(command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
        }

        command_list->SetPipelineState(pso_.get());
        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        ID3D12DescriptorHeap* heaps[] = {
            resources_.shader_manager->get(),
            resources_.sampler_manager->get() };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);

        const VertexLayoutConstants vertex_layout{
            resources_.scene->vertex_layout.position_offset,
            resources_.scene->vertex_layout.texcoord_offset,
            resources_.scene->vertex_layout.normal_offset,
            resources_.scene->vertex_layout.tangent_offset
        };
        command_list->SetGraphicsRoot32BitConstants(
            static_cast<UINT>(RootParam::VERTEX_LAYOUT),
            VERTEX_LAYOUT_DWORD_COUNT, &vertex_layout, 0);
        command_list->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParam::VIEW_CONSTANT),
            resources_.constant_buffers[frame_index]->get()->GetGPUVirtualAddress());
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::VISIBILITY),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_VISIBILITY_BUFFER));
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::INDEX_BUFFER),
            resources_.scene->index_buffer->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::VERTEX_BUFFER),
            resources_.scene->vertex_buffer->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::INSTANCE_BUFFER),
            resources_.scene->instance_buffer->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::SUBMESH_BUFFER),
            resources_.scene->submesh_buffer->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::GEOMETRY_INSTANCE_BUFFER),
            resources_.scene->geometry_instance_buffer->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::MATERIAL_BUFFER),
            resources_.scene->material_buffer->GetGPUVirtualAddress());
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::MATERIAL_TEXTURES),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_MATERIAL_TEXTURE_BEGIN));
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::MATERIAL_SAMPLER),
            resources_.sampler_manager->get_gpu_adr(
                eng::ResourceManagerSampler::EnumDescPos::DONUT_MATERIAL));

        D3D12_CPU_DESCRIPTOR_HANDLE rtvs[4]{};
        rtvs[0] = resources_.frame_manager->get_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_0);
        rtvs[1] = resources_.frame_manager->get_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_1);
        rtvs[2] = resources_.frame_manager->get_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_2);
        rtvs[3] = resources_.frame_manager->get_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_3);

        constexpr float clear[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        for (const D3D12_CPU_DESCRIPTOR_HANDLE& rtv : rtvs) {
            command_list->ClearRenderTargetView(rtv, clear, 0, nullptr);
        }

        command_list->OMSetRenderTargets(4, rtvs, FALSE, nullptr);
        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->DrawInstanced(3, 1, 0, 0);
    }
}
