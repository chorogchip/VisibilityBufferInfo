#include "render/pass/benchmark/PassGBuffer.h"

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"
#include "engine/ResourceViewBuilder.h"
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
            MATERIAL_BUFFER,
            MATERIAL_TEXTURE,
            MATERIAL_SAMPLER,
        };

        struct DrawConstants {
            uint32_t first_instance = 0;
            uint32_t material_id = 0;
        };
    }

    void PassGBuffer::init(ID3D12Device* device, const util::ProgramArgument& arguments,
        const PassGBufferResources& resources, bool use_prepass_depth) {

        resources_ = resources;
        use_prepass_depth_ = use_prepass_depth;

        for (UINT i = 0; i < resources_.gbuffer_count; ++i) {
            resources_.frame_manager->create_rtv(
                static_cast<eng::ResourceManagerFrame::EnumRTV>(
                    static_cast<UINT>(eng::ResourceManagerFrame::EnumRTV::BENCH_GBUFFER_0) + i),
                resources_.gbuffers[i]->get());
        }

        resources_.frame_manager->create_dsv(
            use_prepass_depth_ ?
            eng::ResourceManagerFrame::EnumDSV::DEPTH_READ_ONLY : 
            eng::ResourceManagerFrame::EnumDSV::DEPTH, resources_.depth->get());
        for (UINT i = 0; i < resources_.material_textures.size(); ++i) {
            ID3D12Resource* resource = resources_.material_textures[i]->get();
            resources_.shader_manager->create_srv(
                resource, eng::ResourceViewBuilder::build_srv(
                    resource,
                    eng::ResourceViewBuilder::EnumResourceType::TEXTURE_2D),
                eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_TEXTURE_BEGIN, i);
        }
        for (UINT i = 0; i < resources_.gbuffer_count; ++i) {
            ID3D12Resource* resource = resources_.gbuffers[i]->get();
            resources_.shader_manager->create_srv(
                resource, eng::ResourceViewBuilder::build_srv(
                    resource,
                    eng::ResourceViewBuilder::EnumResourceType::TEXTURE_2D,
                    DXGI_FORMAT_R32G32B32A32_FLOAT),
                eng::ResourceManagerShader::EnumDescPos::BENCH_GBUFFER_0, i);
        }

        auto vs = dxutl::compile_shader(
            L"assets/shaders/deferred_geometry_VS.hlsl",
            L"vs_6_5", L"main", arguments);
        auto ps = dxutl::compile_shader(
            L"assets/shaders/deferred_geometry_PS.hlsl",
            L"ps_6_5", L"main", arguments);

        pso_.init(device);
        pso_.set_graphics();
        auto root_signature = eng::RootSignatureBuilder{}
            .set_flags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
            .root_cbv().reg(0).vis_vtx().add()  // FRAME_CONSTANT
            .constant().reg(1).cnt(2).vis_vtx().add()  // DRAW_CONSTANT
            .root_srv().reg(0).vis_vtx().add()  // INSTANCE_BUFFER
            .root_srv().reg(10).vis_vtx().add()  // DRAW_INSTANCE_BUFFER
            .root_srv().reg(11).vis_vtx().add()  // DRAW_INSTANCE_ID_BUFFER
            .root_srv().reg(1).vis_pxl().add()  // MATERIAL_BUFFER
            .srv_tabl().reg(8).cnt(1).vis_pxl().add()  // MATERIAL_TEXTURE
            .spl_tabl().reg(0).cnt(1).vis_pxl().add()  // MATERIAL_SAMPLER
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_shader_pixel(ps.Get());
        pso_.set_render_targets(resources_.gbuffer_count, DXGI_FORMAT_R32G32B32A32_FLOAT);
        if (use_prepass_depth_)
            pso_.set_depth_equal();
        pso_.build();
    }

    void PassGBuffer::render(ID3D12GraphicsCommandList* command_list, UINT frame_index,
        const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor_rect) {
        for (UINT i = 0; i < resources_.gbuffer_count; ++i)
            resources_.gbuffers[i]->transition(
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
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::MATERIAL_BUFFER),
            resources_.material_buffer_address);
        if (!resources_.to_use_textures) command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::MATERIAL_TEXTURE),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_TEXTURE_BEGIN));
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::MATERIAL_SAMPLER),
            resources_.sampler_manager->get_gpu_adr(
                eng::ResourceManagerSampler::EnumDescPos::BENCH_MATERIAL));

        D3D12_CPU_DESCRIPTOR_HANDLE rtvs[8]{};
        for (UINT i = 0; i < resources_.gbuffer_count; ++i) {
            rtvs[i] = resources_.frame_manager->get_rtv(
                eng::ResourceManagerFrame::EnumRTV::BENCH_GBUFFER_0, +i);
            constexpr float clear[] = { 0.1f, 0.1f, 0.15f, 1.0f };
            command_list->ClearRenderTargetView(rtvs[i], clear, 0, nullptr);
        }
        const auto dsv = resources_.frame_manager->get_dsv(
            use_prepass_depth_ ?
            eng::ResourceManagerFrame::EnumDSV::DEPTH_READ_ONLY :
            eng::ResourceManagerFrame::EnumDSV::DEPTH);
        command_list->OMSetRenderTargets(resources_.gbuffer_count, rtvs, FALSE, &dsv);
        if (!use_prepass_depth_)
            command_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetVertexBuffers(0, 1, &resources_.vertex_buffer_view);
        command_list->IASetIndexBuffer(&resources_.index_buffer_view);
        for (const auto& draw : resources_.draw_stream->draw_calls_compacted) {
            const DrawConstants draw_constants{
                draw.first_instance,
                draw.material_id
            };
            command_list->SetGraphicsRoot32BitConstants(
                static_cast<UINT>(RootParam::DRAW_CONSTANT),
                2, &draw_constants, 0);
            if (resources_.to_use_textures) command_list->SetGraphicsRootDescriptorTable(
                static_cast<UINT>(RootParam::MATERIAL_TEXTURE),
                resources_.shader_manager->get_gpu_adr(
                    eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_TEXTURE_BEGIN,
                    (*resources_.materials)[draw.material_id].texture_indices[0]));
            command_list->DrawIndexedInstanced(
                draw.index_count,
                draw.instance_count,
                draw.index_offset,
                draw.vertex_offset,
                0);
        }
    }

}
