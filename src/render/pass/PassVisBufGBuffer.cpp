#include "render/pass/PassVisBufGBuffer.h"

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "render/RootParameter.h"
#include "render/pass/PassDescriptorRequests.h"
#include "render/renderer/RendererBase.h"

namespace rndr {
    void PassVisBufGBuffer::init(ID3D12Device* device, const util::ProgramArgument& arguments,
        const PassVisBufGBufferResources& resources) {
        resources_ = resources;
        auto& frame = RendererBase::get_resource_manager().frame;
        for (UINT i = 0; i < resources_.gbuffer_count; ++i)
            frame.request_rtv(static_cast<eng::ResourceManagerFrame::EnumRTV>(
                static_cast<UINT>(eng::ResourceManagerFrame::EnumRTV::BENCH_GBUFFER_0) + i), resources_.gbuffers[i]);
        request_visbuf_scene(RendererBase::get_resource_manager().shader, resources_.visbuf);
        D3D12_SHADER_RESOURCE_VIEW_DESC gbuffer_desc{};
        gbuffer_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        gbuffer_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        gbuffer_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        gbuffer_desc.Texture2D.MipLevels = 1;
        for (UINT i = 0; i < resources_.gbuffer_count; ++i)
            RendererBase::get_resource_manager().shader.request(
                eng::ResourceManagerShader::EnumDescPos::BENCH_GBUFFER_0,
                resources_.gbuffers[i], &gbuffer_desc, i);
        auto vs = dxutl::compile_shader(L"assets/shaders/visbuf_lighting_VS.hlsl", "vs_5_0", "main", arguments);
        auto ps = dxutl::compile_shader(L"assets/shaders/visbuf_gbuffer_PS.hlsl", "ps_5_0", "main", arguments);
        pso_.init(device);
        pso_.set_texture_count(arguments.texture_count);
        pso_.set_shaders(vs.Get(), ps.Get());
        pso_.set_fullscreen();
        pso_.set_bench_scene_resolve();
        pso_.set_render_targets(resources_.gbuffer_count, DXGI_FORMAT_R32G32B32A32_FLOAT);
        pso_.build();
    }

    void PassVisBufGBuffer::render(ID3D12GraphicsCommandList* command_list, UINT frame_index,
        const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor_rect) {
        auto& managers = RendererBase::get_resource_manager();
        dxutl::transition_resource(command_list, resources_.visbuf.visibility,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        for (UINT i = 0; i < resources_.gbuffer_count; ++i)
            dxutl::transition_resource(command_list, resources_.gbuffers[i],
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        command_list->SetPipelineState(pso_.get());
        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        ID3D12DescriptorHeap* heaps[] = { managers.shader.get(), resources_.sampler_heap };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);
        command_list->SetGraphicsRootConstantBufferView(root_param(EnumRootParamFullscreen::PASS_CONSTANT),
            resources_.constant_buffers[frame_index]->GetGPUVirtualAddress());
        command_list->SetGraphicsRootDescriptorTable(root_param(EnumRootParamFullscreen::BENCH_INPUT_SRV),
            managers.shader.get_gpu_adr(eng::ResourceManagerShader::EnumDescPos::BENCH_VISIBILITY_BUFFER));
        command_list->SetGraphicsRootDescriptorTable(root_param(EnumRootParamFullscreen::BENCH_MATERIAL_TEXTURE),
            managers.shader.get_gpu_adr(eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_TEXTURE_BEGIN));
        command_list->SetGraphicsRootDescriptorTable(root_param(EnumRootParamFullscreen::BENCH_MATERIAL_SAMPLER),
            resources_.sampler_heap->GetGPUDescriptorHandleForHeapStart());
        command_list->RSSetViewports(1, &viewport);
        command_list->RSSetScissorRects(1, &scissor_rect);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvs[8]{};
        constexpr float clear[] = { .1f, .1f, .15f, 1.f };
        for (UINT i = 0; i < resources_.gbuffer_count; ++i) {
            rtvs[i] = managers.frame.get_rtv(static_cast<eng::ResourceManagerFrame::EnumRTV>(
                static_cast<UINT>(eng::ResourceManagerFrame::EnumRTV::BENCH_GBUFFER_0) + i));
            command_list->ClearRenderTargetView(rtvs[i], clear, 0, nullptr);
        }
        command_list->OMSetRenderTargets(resources_.gbuffer_count, rtvs, FALSE, nullptr);
        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->DrawInstanced(3, 1, 0, 0);
    }
}
