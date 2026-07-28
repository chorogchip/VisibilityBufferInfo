#include "render/pass/donut/PassDonutVisDebugResolve.h"

#include <string>
#include <vector>

#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerShader.h"
#include "engine/ResourceViewBuilder.h"
#include "engine/RootSignatureBuilder.h"

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
        };

        struct VertexLayoutConstants {
            std::uint32_t position_offset = 0;
            std::uint32_t texcoord_offset = 0;
        };

        static constexpr UINT VERTEX_LAYOUT_DWORD_COUNT =
            sizeof(VertexLayoutConstants) / sizeof(std::uint32_t);
    }

    void PassDonutVisDebugResolve::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        VisibilityDebugMode mode,
        const PassDonutVisDebugResolveResources& resources) {

        resources_ = resources;

        resources_.frame_manager->create_rtv(
            eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_0,
            resources_.back_buffers[0]->get());
        resources_.frame_manager->create_rtv(
            eng::ResourceManagerFrame::EnumRTV::BACK_BUFFER_1,
            resources_.back_buffers[1]->get());

        resources_.shader_manager->create_srv(
            resources_.visibility->get(),
            eng::ResourceViewBuilder::build_srv(
                resources_.visibility->get(),
                eng::ResourceViewBuilder::EnumResourceType::TEXTURE_2D,
                DXGI_FORMAT_R32G32_UINT),
            eng::ResourceManagerShader::EnumDescPos::DONUT_VISIBILITY_BUFFER);

        const std::vector<std::wstring> defines = {
            L"VISIBILITY_DEBUG_MODE=" +
            std::to_wstring(static_cast<std::uint32_t>(mode))
        };
        auto vs = dxutl::compile_shader(
            L"assets/shaders/benchmark/visbuf_lighting_VS.hlsl",
            L"vs_6_5", L"main", arguments);
        auto ps = dxutl::compile_shader(
            L"assets/shaders/mydonut/donut_vis_debug_PS.hlsl",
            L"ps_6_5", L"main", defines);

        pso_.init(device);
        pso_.set_graphics();
        auto root_signature = eng::RootSignatureBuilder{}
            .constant().reg(1).cnt(VERTEX_LAYOUT_DWORD_COUNT)
                .spc(1).vis_pxl().add()                 // VERTEX_LAYOUT
            .root_cbv().reg(2).spc(2).vis_pxl().add()  // VIEW_CONSTANT
            .srv_tabl().reg(20).cnt(1).spc(1)
                .vis_pxl().add()                        // VISIBILITY
            .root_srv().reg(21).spc(1).vis_pxl().add() // INDEX_BUFFER
            .root_srv().reg(22).spc(1).vis_pxl().add() // VERTEX_BUFFER
            .root_srv().reg(23).spc(1).vis_pxl().add() // INSTANCE_BUFFER
            .root_srv().reg(24).spc(1).vis_pxl().add() // SUBMESH_BUFFER
            .root_srv().reg(25).spc(1).vis_pxl().add() // GEOMETRY_INSTANCE_BUFFER
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_shader_pixel(ps.Get());
        pso_.set_fullscreen();
        pso_.set_render_targets(1, DXGI_FORMAT_R8G8B8A8_UNORM);
        pso_.build();
    }

    void PassDonutVisDebugResolve::render(
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
        ID3D12DescriptorHeap* heaps[] = { resources_.shader_manager->get() };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);

        const VertexLayoutConstants vertex_layout{
            resources_.scene->vertex_layout.position_offset,
            resources_.scene->vertex_layout.texcoord_offset
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
            resources_.scene->index_buffer.get()->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::VERTEX_BUFFER),
            resources_.scene->vertex_buffer.get()->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::INSTANCE_BUFFER),
            resources_.scene->instance_buffer.get()->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::SUBMESH_BUFFER),
            resources_.scene->submesh_buffer.get()->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParam::GEOMETRY_INSTANCE_BUFFER),
            resources_.scene->geometry_instance_buffer.get()->
                GetGPUVirtualAddress());

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
