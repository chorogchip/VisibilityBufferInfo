#include "render/pass/PassVisBufResolve.h"

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "render/RootParameter.h"
#include "render/pass/PassDescriptorRequests.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerShader.h"

namespace rndr {
    void PassVisBufResolve::init(ID3D12Device* device, const util::ProgramArgument& arguments,
        const PassVisBufResolveResources& resources) {
        resources_ = resources;
        resources_.frame_manager->request_rtv(eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0, resources_.back_buffers[0]);
        resources_.frame_manager->request_rtv(eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_1, resources_.back_buffers[1]);
        request_visbuf_scene(*resources_.shader_manager,
            resources_.vertex_buffer, resources_.index_buffer, resources_.mesh_buffer,
            resources_.instance_buffer, resources_.material_buffer,
            resources_.material_textures, resources_.scene);
        auto vs = dxutl::compile_shader(L"assets/shaders/visbuf_lighting_VS.hlsl", "vs_5_0", "main", arguments);
        auto ps = dxutl::compile_shader(L"assets/shaders/visbuf_lighting_PS.hlsl", "ps_5_0", "main", arguments);
        pso_.init(device);
        pso_.set_texture_count(arguments.texture_count);
        pso_.set_shaders(vs.Get(), ps.Get());
        pso_.set_fullscreen();
        pso_.set_bench_scene_resolve();
        pso_.build();
    }

    void PassVisBufResolve::render(ID3D12GraphicsCommandList* command_list, UINT frame_index,
        const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor_rect) {
        dxutl::transition_resource(command_list, resources_.visibility,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dxutl::transition_resource(command_list, resources_.back_buffers[frame_index],
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        command_list->SetPipelineState(pso_.get());
        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        ID3D12DescriptorHeap* heaps[] = { resources_.shader_manager->get(), resources_.sampler_heap };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);
        command_list->SetGraphicsRootConstantBufferView(root_param(EnumRootParamFullscreen::PASS_CONSTANT),
            resources_.constant_buffers[frame_index]->GetGPUVirtualAddress());
        command_list->SetGraphicsRootDescriptorTable(root_param(EnumRootParamFullscreen::BENCH_INPUT_SRV),
            resources_.shader_manager->get_gpu_adr(eng::ResourceManagerShader::EnumDescPos::BENCH_VISIBILITY_BUFFER));
        command_list->SetGraphicsRootDescriptorTable(root_param(EnumRootParamFullscreen::BENCH_MATERIAL_TEXTURE),
            resources_.shader_manager->get_gpu_adr(eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_TEXTURE_BEGIN));
        command_list->SetGraphicsRootDescriptorTable(root_param(EnumRootParamFullscreen::BENCH_MATERIAL_SAMPLER),
            resources_.sampler_heap->GetGPUDescriptorHandleForHeapStart());
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);
        const auto rtv = resources_.frame_manager->get_rtv(frame_index == 0
            ? eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0
            : eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_1);
        command_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        constexpr float clear[] = { .1f, .1f, .15f, 1.f };
        command_list->ClearRenderTargetView(rtv, clear, 0, nullptr);
        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->DrawInstanced(3, 1, 0, 0);
        dxutl::transition_resource(command_list, resources_.back_buffers[frame_index],
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    }
}
