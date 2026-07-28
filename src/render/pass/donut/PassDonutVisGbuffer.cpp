#include "render/pass/donut/PassDonutVisGbuffer.h"

#include <string>
#include <vector>

#include "dx_util/ShaderUtils.h"
#include "dx_util/ResourceUtils.h"
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
            GBUFFER,
            PIXEL_LIST,
            INDIRECT_CONSTANT,
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
            const scene::DonutSceneGPUData::MaterialData& material =
                resources_.scene->material_data[material_id];
            for (UINT slot_index = 0;
                slot_index < MATERIAL_TEXTURE_DESCRIPTOR_COUNT;
                ++slot_index) {
                const uint32_t texture_index = material.texture_indices[slot_index];
                util::Logger::g_logger.assert_with_log(
                    texture_index < resources_.scene->textures.size(),
                    "Donut visibility G-buffer material texture index is invalid");

                resources_.shader_manager->create_srv(
                    resources_.scene->textures[texture_index].get(),
                    eng::ResourceViewBuilder::build_srv(
                        resources_.scene->textures[texture_index].get(),
                        eng::ResourceViewBuilder::EnumResourceType::TEXTURE_2D),
                    eng::ResourceManagerShader::EnumDescPos::DONUT_MATERIAL_TEXTURE_BEGIN,
                    material_id * MATERIAL_TEXTURE_DESCRIPTOR_COUNT + slot_index);
            }
        }

        resources_.sampler_manager->create_sampler(
            eng::ResourceManagerSampler::EnumDescPos::DONUT_MATERIAL,
            eng::ResourceManagerSampler::EnumSamplerType::LINEAR_WRAP);


        for (int i = 0; i < 4; ++i) {
            resources_.shader_manager->create_uav(
                resources_.gbuffers[i]->get(),
                eng::ResourceViewBuilder::build_uav(
                    resources_.gbuffers[i]->get(),
                    eng::ResourceViewBuilder::EnumResourceType::TEXTURE_2D,
                    util::RenderConstants::DONUT_GBUFFER_NON_SRGB_FORMATS[i]),
                eng::ResourceManagerShader::EnumDescPos::DONUT_GBUFFER_UAV_0,
                i);
        }

        const UINT material_tex_desc_cnt =
            static_cast<UINT>(resources_.scene->material_data.size()) *
            MATERIAL_TEXTURE_DESCRIPTOR_COUNT;

        util::Logger::g_logger.assert_with_log(
            material_tex_desc_cnt <=
            scene::DonutSceneGPUData::MAX_MATERIAL_TEXTURE_DESCRIPTOR_COUNT,
            "Donut visibility G-buffer material texture descriptor count exceeds shader limit");

        const std::vector<std::wstring> cs_defines = {
            std::wstring(L"DONUT_MATERIAL_TEXTURE_DESCRIPTOR_COUNT=") +
                std::to_wstring(material_tex_desc_cnt)
        };

        auto cs = dxutl::compile_shader(
            L"assets/shaders/mydonut/donut_vis_gbuffer_CS.hlsl",
            L"cs_6_5", L"main", cs_defines);

        shader_count_ = 256;
        pso_.init(device);
        pso_.set_shader_count(shader_count_);
        pso_.set_compute();
        auto root_signature = eng::RootSignatureBuilder{}
            .constant().reg(1).cnt(VERTEX_LAYOUT_DWORD_COUNT)
                .spc(1).add()                        // VERTEX_LAYOUT
            .root_cbv().reg(2).spc(2).add()          // VIEW_CONSTANT
            .srv_tabl().reg(20).cnt(1).spc(1).add()  // VISIBILITY
            .root_srv().reg(21).spc(1).add()         // INDEX_BUFFER
            .root_srv().reg(22).spc(1).add()         // VERTEX_BUFFER
            .root_srv().reg(23).spc(1).add()         // INSTANCE_BUFFER
            .root_srv().reg(24).spc(1).add()         // SUBMESH_BUFFER
            .root_srv().reg(25).spc(1).add()         // GEOMETRY_INSTANCE_BUFFER
            .root_srv().reg(26).spc(1).add()         // MATERIAL_BUFFER
            .srv_tabl().reg(0).cnt(material_tex_desc_cnt)
                .spc(0).add()                        // MATERIAL_TEXTURES
            .spl_tabl().reg(0).cnt(1).spc(2).add()   // MATERIAL_SAMPLER
            .uav_tabl().reg(0).cnt(4).add()          // GBUFFER
            .root_srv().reg(27).spc(1).add()         // PIXEL_LIST TODO
            .constant().reg(3).spc(2).cnt(2).add()   // INDIRECT_CONSTANT
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_compute(cs.Get());
        pso_.build();

        D3D12_INDIRECT_ARGUMENT_DESC argument_descs[2]{};
        argument_descs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
        argument_descs[0].Constant.RootParameterIndex =
            static_cast<UINT>(RootParam::INDIRECT_CONSTANT);
        argument_descs[0].Constant.DestOffsetIn32BitValues = 0;
        argument_descs[0].Constant.Num32BitValuesToSet = 2;
        argument_descs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

        dispatch_sig_ = dxutl::create_dispatch_command_signature(
            device,
            argument_descs,
            _countof(argument_descs),
            sizeof(DispatchCommand),
            root_signature.Get());
    }

    void PassDonutVisGBuffer::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index) {

        resources_.visibility->transition(
            command_list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        for (eng::GPUResource* gbuffer : resources_.gbuffers) {
            gbuffer->transition(
                command_list,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
        resources_.pixel_list->transition(
            command_list, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        resources_.indirect_dispatch_list->transition(
            command_list, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

        command_list->SetGraphicsRootSignature(pso_.get_root_signature());
        ID3D12DescriptorHeap* heaps[] = {
            resources_.shader_manager->get(),
            resources_.sampler_manager->get() };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);

        const VertexLayoutConstants vertex_layout{
            resources_.scene->vertex_layout.position_offset,
            resources_.scene->vertex_layout.texcoord_offset,
            resources_.scene->vertex_layout.normal_offset,
            resources_.scene->vertex_layout.tangent_offset
        };
        command_list->SetComputeRoot32BitConstants(
            static_cast<UINT>(RootParam::VERTEX_LAYOUT),
            VERTEX_LAYOUT_DWORD_COUNT, &vertex_layout, 0);
        command_list->SetComputeRootConstantBufferView(
            static_cast<UINT>(RootParam::VIEW_CONSTANT),
            resources_.constant_buffers[frame_index]->get()->GetGPUVirtualAddress());
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParam::VISIBILITY),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_VISIBILITY_BUFFER));
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParam::INDEX_BUFFER),
            resources_.scene->index_buffer.get()->GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParam::VERTEX_BUFFER),
            resources_.scene->vertex_buffer.get()->GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParam::INSTANCE_BUFFER),
            resources_.scene->instance_buffer.get()->GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParam::SUBMESH_BUFFER),
            resources_.scene->submesh_buffer.get()->GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParam::GEOMETRY_INSTANCE_BUFFER),
            resources_.scene->geometry_instance_buffer.get()->
                GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParam::MATERIAL_BUFFER),
            resources_.scene->material_buffer.get()->GetGPUVirtualAddress());
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParam::MATERIAL_TEXTURES),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_MATERIAL_TEXTURE_BEGIN));
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParam::MATERIAL_SAMPLER),
            resources_.sampler_manager->get_gpu_adr(
                eng::ResourceManagerSampler::EnumDescPos::DONUT_MATERIAL));
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParam::GBUFFER),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_GBUFFER_UAV_0));
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParam::PIXEL_LIST),
            resources_.pixel_list->get()->GetGPUVirtualAddress());

        constexpr float clear_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        for (int i = 0; i < 4; ++i) {
            command_list->ClearUnorderedAccessViewFloat(
                resources_.shader_manager->get_gpu_adr(
                    eng::ResourceManagerShader::EnumDescPos::DONUT_GBUFFER_UAV_0,
                    i),
                resources_.shader_manager->get_cpu_adr(
                    eng::ResourceManagerShader::EnumDescPos::DONUT_GBUFFER_UAV_0,
                    i),
                resources_.gbuffers[i]->get(),
                clear_color,
                0,
                nullptr);
        }

        for (int i = 0; i < shader_count_; ++i) {

            command_list->SetPipelineState(pso_.get(i));

            command_list->ExecuteIndirect(
                dispatch_sig_.Get(),
                1,
                resources_.indirect_dispatch_list->get(),
                i * sizeof(DispatchCommand),
                nullptr,
                0);

            for (int i = 0; i < 4; ++i) {
                resources_.gbuffers[i]->uav_barrier(command_list);
            }
        }
    }
}
