#include "render/pass/donut/PassDonutGBuffer.h"

#include "util/Assertion.h"
#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
// Intentionally disabled: another agent is removing PassDescriptorRequests.*.
// #include "render/pass/PassDescriptorRequests.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"
#include "engine/RootSignatureBuilder.h"
#include "util/RenderConstants.h"

namespace rndr {

    namespace {

        enum class RootParam : UINT {
            PUSH_CONSTANT,      // push constant, 7 DWORD (VS, b1, space1)
            VIEW_CONSTANT,      // gbuffer view constant buf (VS +PS, b2, space2)
            GEOMETRY_DATA,      // instances(StrBuf), vertices(BAdrBuf)
                                // (VS, t10~t11, space1)
            MATERIAL_CONSTANT,  // material const buf (PS, b0, space0)
            MATERIAL_TEXTURES,  // base, rough, norm, emisv, occlus, transmit, opac
                                // (PS, t0~t6, space0)
            MATERIAL_SAMPLER,   // (PS, s0, space2)
        };
    }

    void PassDonutGBuffer::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutGBufferResources& resources,
        bool use_prepass_depth) {

        resources_ = resources;
        use_prepass_depth_ = use_prepass_depth;
        use_motion_vectors_ = false;

        // TODO modify desc (structuredbuf, byteaddressbuf)
        resources_.shader_manager->create_srv(
            eng::ResourceManagerShader::EnumDescPos::DONUT_INSTANCE_BUFFER,
            resources_.instance_buffer->get());
        resources_.shader_manager->create_srv(
            eng::ResourceManagerShader::EnumDescPos::DONUT_VERTEX_BUFFER,
            resources_.vertex_buffer->get());

        util::assure_next<
            eng::ResourceManagerShader::EnumDescPos::DONUT_INSTANCE_BUFFER,
            eng::ResourceManagerShader::EnumDescPos::DONUT_VERTEX_BUFFER>();

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
            L"assets/shaders/donut/donut_gbuffer_VS.hlsl",
            "vs_5_1", "buffer_loads", arguments);
        auto ps = dxutl::compile_shader(
            L"assets/shaders/donut/donut_gbuffer_PS.hlsl",
            "ps_5_1", "main", arguments);

        const D3D12_SHADER_VISIBILITY view_vis = use_motion_vectors_ ?
            D3D12_SHADER_VISIBILITY_ALL :
            D3D12_SHADER_VISIBILITY_VERTEX;

        pso_.init(device);
        pso_.set_graphics();
        auto root_signature = eng::RootSignatureBuilder{}
            .constant().reg( 1).cnt(7).spc(1).vis_vtx().add()  // PUSH_CONSTANT
            .root_cbv().reg( 2)   .spc(2).vis(view_vis).add()  // VIEW_CONSTANT
            .srv_tabl().reg(10).cnt(2).spc(1).vis_vtx().add()  // GEOMETRY_DATA
            .root_cbv().reg( 0)       .spc(0).vis_pxl().add()  // MATERIAL_CONSTANT
            .srv_tabl().reg( 0).cnt(7).spc(0).vis_pxl().add()  // MATERIAL_TEXTURES
            .spl_tabl().reg( 0).cnt(1).spc(2).vis_pxl().add()  // MATERIAL_SAMPLER
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_shader_pixel(ps.Get());
        pso_.set_render_targets(
            static_cast<UINT>(util::RenderConstants::DONUT_GBUFFER_FORMATS.size()),
            util::RenderConstants::DONUT_GBUFFER_FORMATS.data());
        if (use_prepass_depth_)
            pso_.set_depth_equal();
        pso_.build();
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

        command_list->SetPipelineState(pso_.get());
        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        ID3D12DescriptorHeap* heaps[] = {
            resources_.shader_manager->get(),
            resources_.sampler_manager->get() };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);

        command_list->SetGraphicsRoot32BitConstants(
            static_cast<UINT>(RootParam::PUSH_CONSTANT),
            7, &push_constants_, 0);
        command_list->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParam::VIEW_CONSTANT),
            resources_.material_constant_buffers[frame_index]->get()->
            GetGPUVirtualAddress()); // TODO aligned offset
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::GEOMETRY_DATA),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_INSTANCE_BUFFER));
        command_list->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParam::MATERIAL_CONSTANT),
            resources_.gbuf_constant_buffers[frame_index]->get()->
            GetGPUVirtualAddress()); // TODO aligned offset
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::MATERIAL_TEXTURES),
            resources_.shader_manager->get_gpu_adr(
            eng::ResourceManagerShader::EnumDescPos::DONUT_GBUFFER_0));  // TODO wrong
        // make pos of PBR texture in srv heap and use this instead gbuffer

        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::MATERIAL_SAMPLER),
            resources_.sampler_manager->get_gpu_adr(
                eng::ResourceManagerSampler::EnumDescPos::DONUT_SHADOW));  // TODO wrong
        // add material sampler

        D3D12_CPU_DESCRIPTOR_HANDLE rtvs[4]{};
        rtvs[0] = resources_.frame_manager->get_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_0);
        rtvs[1] = resources_.frame_manager->get_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_1);
        rtvs[2] = resources_.frame_manager->get_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_2);
        rtvs[3] = resources_.frame_manager->get_rtv(
            eng::ResourceManagerFrame::EnumRTV::DONUT_GBUFFER_3);

        constexpr float clear[] = { 0.1f, 0.1f, 0.15f, 1.0f };
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

        // TODO modify batch when data is correctly set
        //  command_list->DrawInstanced(3, 1, 0, 0);
        for (const auto& batch : resources_.scene->batches) {
            const auto& mesh = resources_.scene->meshes[batch.mesh_index];
            command_list->DrawIndexedInstanced(mesh.index_count, batch.object_count,
                mesh.index_start, mesh.vertex_start, 0);
        }
    }
}
