#include "render/pass/PassVisBufResolve.h"

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "render/pass/PassDescriptorRequests.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"
#include "engine/RootSignatureBuilder.h"

namespace rndr {

    namespace {
        enum class RootParam : UINT {
            PASS_CONSTANT,
            SCENE_INPUT,
            MATERIAL_TEXTURE,
            MATERIAL_SAMPLER,
        };
    }

    void PassVisBufResolve::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassVisBufResolveResources& resources) {

        resources_ = resources;

        resources_.frame_manager->create_rtv(
            eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0,
            resources_.back_buffers[0]->get());
        resources_.frame_manager->create_rtv(
            eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_1,
            resources_.back_buffers[1]->get());

        request_visbuf_scene(
            *resources_.shader_manager,
            resources_.vertex_buffer,
            resources_.index_buffer,
            resources_.mesh_buffer,
            resources_.instance_buffer,
            resources_.material_buffer,
            resources_.material_textures,
            resources_.scene);

        auto vs = dxutl::compile_shader(
            L"assets/shaders/visbuf_lighting_VS.hlsl",
            "vs_5_0", "main", arguments);
        auto ps = dxutl::compile_shader(
            L"assets/shaders/visbuf_lighting_PS.hlsl",
            "ps_5_0", "main", arguments);

        pso_.init(device);
        auto root_signature = eng::RootSignatureBuilder{}
            .root_cbv().reg(0)
                .vis(D3D12_SHADER_VISIBILITY_PIXEL).add()  // PASS_CONSTANT
            .srv_tabl().reg(0).cnt(6)
                .vis(D3D12_SHADER_VISIBILITY_PIXEL).add()  // SCENE_INPUT
            .srv_tabl().reg(8).cnt(arguments.texture_count)
                .vis(D3D12_SHADER_VISIBILITY_PIXEL).add()  // MATERIAL_TEXTURE
            .spl_tabl().reg(0).cnt(1)
                .vis(D3D12_SHADER_VISIBILITY_PIXEL).add()  // MATERIAL_SAMPLER
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_shader_pixel(ps.Get());
        pso_.set_fullscreen();
        pso_.build();
    }

    void PassVisBufResolve::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {

        resources_.visibility->transition(
            command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        resources_.back_buffers[frame_index]->transition(
            command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
        command_list->SetPipelineState(pso_.get());
        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        ID3D12DescriptorHeap* heaps[] = {
            resources_.shader_manager->get(),
            resources_.sampler_manager->get() };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);
        command_list->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParam::PASS_CONSTANT),
            resources_.constant_buffers[frame_index]->GetGPUVirtualAddress());
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::SCENE_INPUT),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::BENCH_VISIBILITY_BUFFER));
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::MATERIAL_TEXTURE),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_TEXTURE_BEGIN));
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::MATERIAL_SAMPLER),
            resources_.sampler_manager->get_gpu_adr(
                eng::ResourceManagerSampler::EnumDescPos::BENCH_MATERIAL));
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);
        const auto rtv = resources_.frame_manager->get_rtv(
            eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0, frame_index);
        command_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        constexpr float clear[] = { 0.1f, 0.1f, 0.15f, 1.0f };
        command_list->ClearRenderTargetView(rtv, clear, 0, nullptr);
        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->DrawInstanced(3, 1, 0, 0);
        resources_.back_buffers[frame_index]->transition(
            command_list, D3D12_RESOURCE_STATE_PRESENT);
    }
}
