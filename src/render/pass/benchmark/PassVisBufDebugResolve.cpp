#include "render/pass/benchmark/PassVisBufDebugResolve.h"

#include <string>
#include <vector>

#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerShader.h"
#include "engine/ResourceViewBuilder.h"
#include "engine/RootSignatureBuilder.h"
#include "util/Assertion.h"

namespace rndr {

    namespace {
        enum class RootParam : UINT {
            PASS_CONSTANT,
            SCENE_INPUT,
        };
    }

    void PassVisBufDebugResolve::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        VisibilityDebugMode mode,
        const PassVisBufDebugResolveResources& resources) {

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
            eng::ResourceManagerShader::EnumDescPos::BENCH_VISIBILITY_BUFFER);

        D3D12_SHADER_RESOURCE_VIEW_DESC scene_desc{};
        scene_desc.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        scene_desc.Format = DXGI_FORMAT_UNKNOWN;
        scene_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        scene_desc.Buffer.FirstElement = 0;
        scene_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        scene_desc.Buffer.StructureByteStride =
            sizeof(resources_.scene->vertices[0]);
        scene_desc.Buffer.NumElements =
            static_cast<UINT>(resources_.scene->vertices.size());
        resources_.shader_manager->create_srv(
            resources_.vertex_buffer->get(), scene_desc,
            eng::ResourceManagerShader::EnumDescPos::BENCH_VERTEX_BUFFER);

        scene_desc.Buffer.StructureByteStride =
            sizeof(resources_.scene->indices[0]);
        scene_desc.Buffer.NumElements =
            static_cast<UINT>(resources_.scene->indices.size());
        resources_.shader_manager->create_srv(
            resources_.index_buffer->get(), scene_desc,
            eng::ResourceManagerShader::EnumDescPos::BENCH_INDEX_BUFFER);

        scene_desc.Buffer.StructureByteStride =
            sizeof(scene::BenchmarkSceneGPUData::SubmeshData);
        scene_desc.Buffer.NumElements =
            static_cast<UINT>(resources_.scene->submeshes.size());
        resources_.shader_manager->create_srv(
            resources_.submesh_buffer->get(), scene_desc,
            eng::ResourceManagerShader::EnumDescPos::BENCH_MESH_BUFFER);

        scene_desc.Buffer.StructureByteStride =
            sizeof(scene::BenchmarkSceneGPUData::InstanceData);
        scene_desc.Buffer.NumElements =
            static_cast<UINT>(resources_.scene->instances.size());
        resources_.shader_manager->create_srv(
            resources_.instance_buffer->get(), scene_desc,
            eng::ResourceManagerShader::EnumDescPos::BENCH_INSTANCE_BUFFER);

        scene_desc.Buffer.StructureByteStride =
            sizeof(scene::BenchmarkSceneGPUData::DrawInstanceData);
        scene_desc.Buffer.NumElements =
            static_cast<UINT>(resources_.scene->draw_instances.size());
        resources_.shader_manager->create_srv(
            resources_.draw_instance_buffer->get(), scene_desc,
            eng::ResourceManagerShader::EnumDescPos::BENCH_DRAW_INSTANCE_BUFFER);

        util::assure_next<
            eng::ResourceManagerShader::EnumDescPos::BENCH_VISIBILITY_BUFFER,
            eng::ResourceManagerShader::EnumDescPos::BENCH_VERTEX_BUFFER,
            eng::ResourceManagerShader::EnumDescPos::BENCH_INDEX_BUFFER,
            eng::ResourceManagerShader::EnumDescPos::BENCH_MESH_BUFFER,
            eng::ResourceManagerShader::EnumDescPos::BENCH_INSTANCE_BUFFER,
            eng::ResourceManagerShader::EnumDescPos::BENCH_DRAW_INSTANCE_BUFFER>();

        const std::vector<std::wstring> defines = {
            L"VISIBILITY_DEBUG_MODE=" +
            std::to_wstring(static_cast<std::uint32_t>(mode))
        };
        auto vs = dxutl::compile_shader(
            L"assets/shaders/benchmark/visbuf_lighting_VS.hlsl",
            L"vs_6_5", L"main", arguments);
        auto ps = dxutl::compile_shader(
            L"assets/shaders/benchmark/visbuf_debug_resolve_PS.hlsl",
            L"ps_6_5", L"main", defines);

        pso_.init(device);
        pso_.set_graphics();
        auto root_signature = eng::RootSignatureBuilder{}
            .root_cbv().reg(0).vis_pxl().add()       // PASS_CONSTANT
            .srv_tabl().reg(0).cnt(6).vis_pxl().add() // SCENE_INPUT
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_vertex(vs.Get());
        pso_.set_shader_pixel(ps.Get());
        pso_.set_fullscreen();
        pso_.build();
    }

    void PassVisBufDebugResolve::render(
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
        command_list->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParam::PASS_CONSTANT),
            resources_.constant_buffer_addresses[frame_index]);
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParam::SCENE_INPUT),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::BENCH_VISIBILITY_BUFFER));

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
