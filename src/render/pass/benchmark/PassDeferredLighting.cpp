#include "render/pass/benchmark/PassDeferredLighting.h"

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
            GBUFFER_INPUT,
        };
    }

    void PassDeferredLighting::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDeferredLightingResources& resources) {

        resources_ = resources;

        resources_.frame_manager->create_rtv(
            eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0,
            resources_.back_buffers[0]->get());

        resources_.frame_manager->create_rtv(
            eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_1,
            resources_.back_buffers[1]->get());
        
        auto vs = dxutl::compile_shader(
            L"assets/shaders/deferred_lighting_VS.hlsl",
            "vs_5_0", "main", arguments);

        auto ps = dxutl::compile_shader(
            L"assets/shaders/deferred_lighting_PS.hlsl",
            "ps_5_0", "main", arguments);

        pso_.init(device);
        pso_.set_graphics();
        auto root_signature = eng::RootSignatureBuilder{}
            .srv_tabl().reg(0).cnt(resources_.gbuffer_count)
            .vis(D3D12_SHADER_VISIBILITY_PIXEL).add()  // // GBUFFER_INPUT
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_shader_pixel(ps.Get());
        pso_.set_fullscreen();
        pso_.build();
    }
    
    void PassDeferredLighting::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor_rect) {

        resources_.back_buffers[frame_index]->transition(
            command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);

        for (UINT i = 0; i < resources_.gbuffer_count; ++i)
            resources_.gbuffers[i]->transition(
                command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        command_list->SetPipelineState(pso_.get());
        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        ID3D12DescriptorHeap* heaps[] = { resources_.shader_manager->get() };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::GBUFFER_INPUT),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::BENCH_GBUFFER_0));
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);
        const auto rtv = resources_.frame_manager->get_rtv(
            eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0, frame_index);
        command_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        constexpr float clear[] = { 0.1f, 0.1f, 0.15f, 1.0f };
        command_list->ClearRenderTargetView(rtv, clear, 0, nullptr);
        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->DrawInstanced(3, 1, 0, 0);
    }
}
