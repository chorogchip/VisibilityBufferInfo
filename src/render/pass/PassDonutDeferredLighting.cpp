#include "render/pass/PassDonutDeferredLighting.h"

#include "util/Assertion.h"
#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerShader.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/RootSignatureBuilder.h"

namespace rndr {

    namespace {
        enum class RootParam : UINT {
            CONSTANT_BUFFER,
            SM_LIGHT_ENVBRDF,
            SAMPLER,
            DEPTH_GBUFFER,
            IBL_SHADOW_AO,
            OUTPUT,
        };
    }

    void PassDonutDeferredLighting::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutDeferredLightingResources& resources) {

        resources_ = resources;

        // TODO adjust exact desc info

        using SRVDescPos = eng::ResourceManagerShader::EnumDescPos;

        resources_.shader_manager->create_srv(
            SRVDescPos::DONUT_SHADOW_MAP_ARRAY,
            resources_.buf_shadow_map->get());
        resources_.shader_manager->create_srv(
            SRVDescPos::DONUT_DIFFUSE_LIGHT_PROBE,
            resources_.buf_diffuse_light_probe->get());
        resources_.shader_manager->create_srv(
            SRVDescPos::DONUT_SPECULAR_LIGHT_PROBE,
            resources_.buf_specular_light_probe->get());
        resources_.shader_manager->create_srv(
            SRVDescPos::DONUT_ENVIRONMENT_BRDF,
            resources_.buf_env_brdf->get());

        util::assure_contiguous<
            SRVDescPos::DONUT_SHADOW_MAP_ARRAY,
            SRVDescPos::DONUT_DIFFUSE_LIGHT_PROBE,
            SRVDescPos::DONUT_SPECULAR_LIGHT_PROBE,
            SRVDescPos::DONUT_ENVIRONMENT_BRDF>();

        D3D12_SAMPLER_DESC sampler_desc{};
        sampler_desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.MaxAnisotropy = 1;
        sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;

        resources_.sampler_manager->create_sampler(
            eng::ResourceManagerSampler::EnumDescPos::DONUT_SHADOW,
            sampler_desc);
        resources_.sampler_manager->create_sampler(
            eng::ResourceManagerSampler::EnumDescPos::DONUT_SHADOW_COMPARISON,
            sampler_desc);
        resources_.sampler_manager->create_sampler(
            eng::ResourceManagerSampler::EnumDescPos::DONUT_LIGHT_PROBE,
            sampler_desc);
        resources_.sampler_manager->create_sampler(
            eng::ResourceManagerSampler::EnumDescPos::DONUT_BRDF,
            sampler_desc);

        util::assure_contiguous<
            eng::ResourceManagerSampler::EnumDescPos::DONUT_SHADOW,
            eng::ResourceManagerSampler::EnumDescPos::DONUT_SHADOW_COMPARISON,
            eng::ResourceManagerSampler::EnumDescPos::DONUT_LIGHT_PROBE,
            eng::ResourceManagerSampler::EnumDescPos::DONUT_BRDF>();

        resources_.shader_manager->create_srv(
            SRVDescPos::DONUT_GBUFFER_DEPTH, resources_.depth->get());
        resources_.shader_manager->create_srv(
            SRVDescPos::DONUT_GBUFFER_0, resources_.gbuffers[0]->get());
        resources_.shader_manager->create_srv(
            SRVDescPos::DONUT_GBUFFER_1, resources_.gbuffers[1]->get());
        resources_.shader_manager->create_srv(
            SRVDescPos::DONUT_GBUFFER_2, resources_.gbuffers[2]->get());
        resources_.shader_manager->create_srv(
            SRVDescPos::DONUT_GBUFFER_3, resources_.gbuffers[3]->get());

        util::assure_contiguous<
            SRVDescPos::DONUT_GBUFFER_DEPTH,
            SRVDescPos::DONUT_GBUFFER_0,
            SRVDescPos::DONUT_GBUFFER_1,
            SRVDescPos::DONUT_GBUFFER_2,
            SRVDescPos::DONUT_GBUFFER_3>();

        resources_.shader_manager->create_srv(
            SRVDescPos::DONUT_INDIRECT_DIFFUSE, resources_.buf_ibl_diffuse->get());
        resources_.shader_manager->create_srv(
            SRVDescPos::DONUT_INDIRECT_SPECULAR, resources_.buf_ibl_specular->get());
        resources_.shader_manager->create_srv(
            SRVDescPos::DONUT_SHADOW_CHANNELS, resources_.buf_shadow->get());
        resources_.shader_manager->create_srv(
            SRVDescPos::DONUT_AMBIENT_OCCLUSION, resources_.buf_ao->get());

        util::assure_contiguous<
            SRVDescPos::DONUT_INDIRECT_DIFFUSE,
            SRVDescPos::DONUT_INDIRECT_SPECULAR,
            SRVDescPos::DONUT_SHADOW_CHANNELS,
            SRVDescPos::DONUT_AMBIENT_OCCLUSION>();

        resources_.shader_manager->create_uav(
            SRVDescPos::DONUT_HDR_COLOR_UAV, resources_.uav_output->get());

        auto cs = dxutl::compile_shader(
            L"assets/shaders/donut_deferred_lighting_CS_copy.hlsl",
            "cs_5_0", "main", arguments
        );

        pso_.init(device);
        auto root_signature = eng::RootSignatureBuilder{}
            .root_cbv().reg(0).add()          // CONSTANT_BUFFER  (b0)
            .srv_tabl().reg(0).cnt(4).add()   // SM_LIGHT_ENVBRDF (t0 t1 t2 t3)
            .spl_tabl().reg(0).cnt(4).add()   // SAMPLER          (s0 s1 s2 s3)
            .srv_tabl().reg(8).cnt(5).add()   // DEPTH_GBUFFER    (t8 t9 t10 t11 t12)
            .srv_tabl().reg(14).cnt(4).add()  // IBL_SHADOW_AO    (t14 t15 t16 t17)
            .uav_tabl().reg(0).cnt(1).add()   // OUTPUT           (u0)
            .build(device);
        pso_.set_root_signature(root_signature.Get());
        pso_.set_shader_compute(cs.Get());
        pso_.build();
    }

    void PassDonutDeferredLighting::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        UINT width,
        UINT height) {

        constexpr auto SRV_STATE = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        resources_.buf_shadow_map->transition(command_list, SRV_STATE);
        resources_.buf_diffuse_light_probe->transition(command_list, SRV_STATE);
        resources_.buf_specular_light_probe->transition(command_list, SRV_STATE);
        resources_.buf_env_brdf->transition(command_list, SRV_STATE);
        resources_.depth->transition(command_list, SRV_STATE);
        resources_.gbuffers[0]->transition(command_list, SRV_STATE);
        resources_.gbuffers[1]->transition(command_list, SRV_STATE);
        resources_.gbuffers[2]->transition(command_list, SRV_STATE);
        resources_.gbuffers[3]->transition(command_list, SRV_STATE);
        resources_.buf_ibl_diffuse->transition(command_list, SRV_STATE);
        resources_.buf_ibl_specular->transition(command_list, SRV_STATE);
        resources_.buf_shadow->transition(command_list, SRV_STATE);
        resources_.buf_ao->transition(command_list, SRV_STATE);
        resources_.uav_output->transition(command_list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        
        command_list->SetPipelineState(pso_.get());
        command_list->SetComputeRootSignature(pso_.get_root_signature());
        ID3D12DescriptorHeap* heaps[] = {
            resources_.shader_manager->get(),
            resources_.sampler_manager->get() };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);


        command_list->SetComputeRootConstantBufferView(
            static_cast<UINT>(RootParam::CONSTANT_BUFFER),
            resources_.constant_buffers[frame_index]->get()->GetGPUVirtualAddress());
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParam::SM_LIGHT_ENVBRDF),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_SHADOW_MAP_ARRAY));
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParam::SAMPLER),
            resources_.sampler_manager->get_gpu_adr(
                eng::ResourceManagerSampler::EnumDescPos::DONUT_SHADOW));
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParam::DEPTH_GBUFFER),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_GBUFFER_DEPTH));
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParam::IBL_SHADOW_AO),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_INDIRECT_DIFFUSE));
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParam::OUTPUT),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_HDR_COLOR_UAV));

        const UINT group_x = (width + 15) / 16;
        const UINT group_y = (height + 15) / 16;
        command_list->Dispatch(group_x, group_y, 1);
    }
}