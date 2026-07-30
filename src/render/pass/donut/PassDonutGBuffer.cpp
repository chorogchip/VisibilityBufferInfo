#include "render/pass/donut/PassDonutGBuffer.h"

#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"
#include "engine/ResourceViewBuilder.h"
#include "engine/RootSignatureBuilder.h"
#include "util/Assertion.h"
#include "util/Logger.h"
#include "util/RenderConstants.h"

namespace rndr {

    namespace {

        enum class RootParam : UINT {
            PUSH_CONSTANT,
            VIEW_CONSTANT,
            MATERIAL_CONSTANT,
            INSTANCE_BUFFER,
            VERTEX_BUFFER,
            DRAW_INSTANCE_BUFFER,
            DRAW_INSTANCE_ID_BUFFER,
            MATERIAL_TEXTURES,
            MATERIAL_SAMPLER,
        };

        enum class JunglePointRootParam : UINT {
            PUSH_CONSTANT,
            VIEW_CONSTANT,
            MATERIAL_CONSTANT,
            VERTEX_BUFFER,
            POINT_INSTANCE_BUFFER,
            POINT_PROTOTYPE_BUFFER,
            POINT_INSTANCE_ID_BUFFER,
            MATERIAL_TEXTURES,
            MATERIAL_SAMPLER,
        };

        struct PushConstants {
            uint32_t start_instance_location = 0;
            uint32_t start_vertex_location = 0;
            uint32_t position_offset = 0;
            uint32_t prev_position_offset = 0;
            uint32_t texcoord_offset = 0;
            uint32_t normal_offset = 0;
            uint32_t tangent_offset = 0;
        };

        struct JunglePointPushConstants {
            uint32_t start_instance_location = 0;
            uint32_t prototype_id = 0;
            uint32_t position_offset = 0;
            uint32_t prev_position_offset = 0;
            uint32_t texcoord_offset = 0;
            uint32_t normal_offset = 0;
            uint32_t tangent_offset = 0;
            uint32_t pad0 = 0;
        };
    }

    static constexpr UINT PUSH_CONSTANT_DWORD_COUNT =
        sizeof(PushConstants) / sizeof(uint32_t);
    static constexpr UINT JUNGLE_POINT_PUSH_CONSTANT_DWORD_COUNT =
        sizeof(JunglePointPushConstants) / sizeof(uint32_t);

    void PassDonutGBuffer::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutGBufferResources& resources,
        bool use_prepass_depth) {

        resources_ = resources;
        use_prepass_depth_ = use_prepass_depth;
        use_motion_vectors_ = false;
        shader_count_ = resources_.scene->active_material_class_count;
        util::Logger::g_logger.assert_with_log(
            (resources_.jungle_scene == nullptr) ==
                (resources_.jungle_draw_stream == nullptr),
            "Donut G-buffer Jungle point resources are incomplete.");

        for (UINT material_id = 0;
            material_id < resources_.scene->material_data.size();
            ++material_id) {
            const scene::DonutSceneGPUData::MaterialData& material =
                resources_.scene->material_data[material_id];
            for (UINT slot_index = 0; slot_index < MATERIAL_TEXTURE_DESCRIPTOR_COUNT; ++slot_index) {
                const uint32_t texture_index =
                    material.texture_indices[slot_index];

                util::Logger::g_logger.assert_with_log(
                    texture_index < resources_.scene->textures.size(),
                    "Donut material texture index is invalid");

                resources_.shader_manager->create_srv(
                    resources_.scene->textures[texture_index].get(),
                    eng::ResourceViewBuilder::build_srv(resources_.scene->textures[texture_index].get(),
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

        util::assure_next<
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_0,
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_1,
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_2,
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_3>();

        resources_.frame_manager->create_dsv(
            use_prepass_depth ?
            eng::ResourceManagerFrame::EnumDSV::DEPTH_READ_ONLY :
            eng::ResourceManagerFrame::EnumDSV::DEPTH,
            resources_.depth->get());

        auto vs = dxutl::compile_shader(
            L"assets/shaders/mydonut/donut_gbuffer_VS.hlsl",
            L"vs_6_5", L"main", arguments);
        auto ps = dxutl::compile_shader(
            L"assets/shaders/mydonut/donut_gbuffer_PS.hlsl",
            L"ps_6_5", L"main", arguments);

        pso_.init(device);
        pso_.set_graphics();

        eng::RootSignatureBuilder root_builder{};
        root_builder
            .set_flags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
            .constant().reg( 1).cnt(PUSH_CONSTANT_DWORD_COUNT).spc(1).vis_vtx().add();
        if (use_motion_vectors_) {
            root_builder.root_cbv().reg( 2).spc(2).vis_all().add();
        } else {
            root_builder.root_cbv().reg( 2).spc(2).vis_vtx().add();
        }

        auto root_signature = root_builder
            .root_cbv().reg( 0)       .spc(0).vis_pxl().add()
            .root_srv().reg(10).spc(1).vis_vtx().add()
            .root_srv().reg(11).spc(1).vis_vtx().add()
            .root_srv().reg(12).spc(1).vis_vtx().add()
            .root_srv().reg(13).spc(1).vis_vtx().add()
            .srv_tabl().reg( 0).cnt(MATERIAL_TEXTURE_DESCRIPTOR_COUNT).spc(0).vis_pxl().add()
            .spl_tabl().reg( 0).cnt(1).spc(2).vis_pxl().add()
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_shader_pixel(ps.Get());
        pso_.set_manual_vertex_fetch();
        const auto& gbuffer_formats = arguments.donut_linear_gbuffer
            ? util::RenderConstants::DONUT_GBUFFER_NON_SRGB_FORMATS
            : util::RenderConstants::DONUT_GBUFFER_FORMATS;
        pso_.set_render_targets(
            static_cast<UINT>(gbuffer_formats.size()),
            gbuffer_formats.data());
        if (use_prepass_depth_)
            pso_.set_depth_equal();
        if (shader_count_)
            pso_.set_shader_count(shader_count_);
        pso_.build();

        if (resources_.jungle_scene != nullptr) {
            auto jungle_vs = dxutl::compile_shader(
                L"assets/shaders/mydonut/jungle_point_gbuffer_VS.hlsl",
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
                    .root_cbv().reg(0).spc(0).vis_pxl().add()
                    .root_srv().reg(11).spc(1).vis_vtx().add()
                    .root_srv().reg(14).spc(1).vis_vtx().add()
                    .root_srv().reg(15).spc(1).vis_vtx().add()
                    .root_srv().reg(16).spc(1).vis_vtx().add()
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
            jungle_point_pso_.set_render_targets(
                static_cast<UINT>(gbuffer_formats.size()),
                gbuffer_formats.data());
            if (use_prepass_depth_) {
                jungle_point_pso_.set_depth_equal();
            }
            if (shader_count_)
                jungle_point_pso_.set_shader_count(shader_count_);
            jungle_point_pso_.build();
        }
    }

    void PassDonutGBuffer::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {

        resources_.gbuffers[0]->transition(
            command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
        resources_.gbuffers[1]->transition(
            command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
        resources_.gbuffers[2]->transition(
            command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
        resources_.gbuffers[3]->transition(
            command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);

        if (use_prepass_depth_)
            resources_.depth->transition(
                command_list, D3D12_RESOURCE_STATE_DEPTH_READ);
        else
            resources_.depth->transition(
                command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);

        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        ID3D12DescriptorHeap* heaps[] = {
            resources_.shader_manager->get(),
            resources_.sampler_manager->get() };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);

        command_list->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParam::VIEW_CONSTANT),
            resources_.constant_buffers[frame_index]->get()->
            GetGPUVirtualAddress());
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
        command_list->ClearRenderTargetView(rtvs[0], clear, 0, nullptr);
        command_list->ClearRenderTargetView(rtvs[1], clear, 0, nullptr);
        command_list->ClearRenderTargetView(rtvs[2], clear, 0, nullptr);
        command_list->ClearRenderTargetView(rtvs[3], clear, 0, nullptr);

        const auto dsv = resources_.frame_manager->get_dsv(
            use_prepass_depth_ ?
            eng::ResourceManagerFrame::EnumDSV::DEPTH_READ_ONLY :
            eng::ResourceManagerFrame::EnumDSV::DEPTH);

        command_list->OMSetRenderTargets(4, rtvs, FALSE, &dsv);

        if (!use_prepass_depth_)
            command_list->ClearDepthStencilView(
                dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetIndexBuffer(&resources_.scene->index_buffer_view);

        std::uint32_t current_shader_id = UINT32_MAX;
        for (const auto& draw :
            resources_.draw_stream->draw_calls_compacted) {
            util::Logger::g_logger.assert_with_log(
                draw.material_id < resources_.scene->material_data.size(),
                "Donut G-buffer draw material ID is invalid.");
            const std::uint32_t shader_id =
                resources_.scene->material_data[
                    draw.material_id].virtual_shader_id;
            util::Logger::g_logger.assert_with_log(
                !shader_count_ || shader_id < shader_count_,  // due to donut
                "Donut G-buffer material class ID is invalid.");
            if (shader_id != current_shader_id) {
                command_list->SetPipelineState(pso_.get(shader_id));
                current_shader_id = shader_id;
            }

            const PushConstants push_constants{
                draw.first_instance,
                0,
                resources_.scene->vertex_layout.position_offset,
                resources_.scene->vertex_layout.prev_position_offset,
                resources_.scene->vertex_layout.texcoord_offset,
                resources_.scene->vertex_layout.normal_offset,
                resources_.scene->vertex_layout.tangent_offset
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

        current_shader_id = UINT32_MAX;
        for (const auto& draw :
            resources_.jungle_draw_stream->
                point_draw_calls_compacted) {
            util::Logger::g_logger.assert_with_log(
                draw.material_id < resources_.scene->material_data.size(),
                "Donut Jungle G-buffer draw material ID is invalid.");
            const std::uint32_t shader_id =
                resources_.scene->material_data[
                    draw.material_id].virtual_shader_id;
            util::Logger::g_logger.assert_with_log(
                !shader_count_ || shader_id < shader_count_,  // due to donut
                "Donut Jungle G-buffer material class ID is invalid.");
            if (shader_id != current_shader_id) {
                command_list->SetPipelineState(
                    jungle_point_pso_.get(shader_id));
                current_shader_id = shader_id;
            }

            const JunglePointPushConstants push_constants{
                draw.first_instance,
                draw.prototype_id,
                resources_.scene->vertex_layout.position_offset,
                resources_.scene->vertex_layout.prev_position_offset,
                resources_.scene->vertex_layout.texcoord_offset,
                resources_.scene->vertex_layout.normal_offset,
                resources_.scene->vertex_layout.tangent_offset,
                0
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
