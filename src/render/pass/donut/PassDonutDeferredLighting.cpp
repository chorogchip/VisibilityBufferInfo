#include "render/pass/donut/PassDonutDeferredLighting.h"

#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"
#include "engine/ResourceViewBuilder.h"
#include "engine/RootSignatureBuilder.h"
#include "util/Assertion.h"
#include "util/RenderConstants.h"

namespace rndr {

    namespace {

        enum class RootParam : UINT {
            CONSTANT_BUFFER,  // c0
            SM_LIGHT_ENVBRDF,  // t0 t1 t2 t3
            SAMPLER,  // s0 s1 s2 s3
            DEPTH_GBUFFER,  // t8 t9 t10 t11 t12
            IBL_SHADOW_AO,  //  t14 t15 t16 t17
            OUTPUT,  // u0
        };
    }

    void PassDonutDeferredLighting::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutDeferredLightingResources& resources) {

        resources_ = resources;

        using SRVDescPos = eng::ResourceManagerShader::EnumDescPos;
        using SamplerDescPos = eng::ResourceManagerSampler::EnumDescPos;

        resources_.shader_manager->create_srv(
            resources_.buf_shadow_map->get(),
            eng::ResourceViewBuilder::build_srv(
                resources_.buf_shadow_map->get(),
                eng::ResourceViewBuilder::EnumResourceType::ARRAY_2D,
                util::RenderConstants::DONUT_DEPTH_SRV_FORMAT),
            SRVDescPos::DONUT_SHADOW_MAP_ARRAY);
        resources_.shader_manager->create_srv(
            resources_.buf_diffuse_light_probe->get(),
            eng::ResourceViewBuilder::build_srv(
                resources_.buf_diffuse_light_probe->get(),
                eng::ResourceViewBuilder::EnumResourceType::CUBEMAP),
            SRVDescPos::DONUT_DIFFUSE_LIGHT_PROBE);
        resources_.shader_manager->create_srv(
            resources_.buf_specular_light_probe->get(),
            eng::ResourceViewBuilder::build_srv(
                resources_.buf_specular_light_probe->get(),
                eng::ResourceViewBuilder::EnumResourceType::CUBEMAP),
            SRVDescPos::DONUT_SPECULAR_LIGHT_PROBE);
        resources_.shader_manager->create_srv(
            resources_.buf_env_brdf->get(),
            eng::ResourceViewBuilder::build_srv(
                resources_.buf_env_brdf->get(),
                eng::ResourceViewBuilder::EnumResourceType::ARRAY_2D),
            SRVDescPos::DONUT_ENVIRONMENT_BRDF);

        util::assure_next<
            SRVDescPos::DONUT_SHADOW_MAP_ARRAY,
            SRVDescPos::DONUT_DIFFUSE_LIGHT_PROBE,
            SRVDescPos::DONUT_SPECULAR_LIGHT_PROBE,
            SRVDescPos::DONUT_ENVIRONMENT_BRDF>();

        resources_.sampler_manager->create_sampler(
            SamplerDescPos::DONUT_SHADOW,
            eng::ResourceManagerSampler::EnumSamplerType::LINEAR_BORDER_WHITE);
        resources_.sampler_manager->create_sampler(
            SamplerDescPos::DONUT_SHADOW_COMPARISON,
            eng::ResourceManagerSampler::EnumSamplerType::LINEAR_BORDER_WHITE_COMP);
        resources_.sampler_manager->create_sampler(
            SamplerDescPos::DONUT_LIGHT_PROBE,
            eng::ResourceManagerSampler::EnumSamplerType::LINEAR_WRAP);
        resources_.sampler_manager->create_sampler(
            SamplerDescPos::DONUT_BRDF,
            eng::ResourceManagerSampler::EnumSamplerType::LINEAR_CLAMP);

        util::assure_next<
            SamplerDescPos::DONUT_SHADOW,
            SamplerDescPos::DONUT_SHADOW_COMPARISON,
            SamplerDescPos::DONUT_LIGHT_PROBE,
            SamplerDescPos::DONUT_BRDF>();

        resources_.shader_manager->create_srv(
            resources_.depth->get(),
            eng::ResourceViewBuilder::build_srv(
                resources_.depth->get(),
                eng::ResourceViewBuilder::EnumResourceType::ARRAY_2D,
                util::RenderConstants::DONUT_DEPTH_SRV_FORMAT),
            SRVDescPos::DONUT_GBUFFER_DEPTH);

        for (UINT i = 0; i < _countof(resources_.gbuffers); ++i) {
            ID3D12Resource* resource = resources_.gbuffers[i]->get();
            resources_.shader_manager->create_srv(
                resource,
                eng::ResourceViewBuilder::build_srv(
                    resource,
                    eng::ResourceViewBuilder::EnumResourceType::ARRAY_2D),
                SRVDescPos::DONUT_GBUFFER_0,
                i);
        }

        util::assure_next<
            SRVDescPos::DONUT_GBUFFER_DEPTH,
            SRVDescPos::DONUT_GBUFFER_0,
            SRVDescPos::DONUT_GBUFFER_1,
            SRVDescPos::DONUT_GBUFFER_2,
            SRVDescPos::DONUT_GBUFFER_3>();

        const eng::GPUResource* auxiliary_resources[] = {
            resources_.buf_ibl_diffuse,
            resources_.buf_ibl_specular,
            resources_.buf_shadow,
            resources_.buf_ao,
        };

        for (UINT i = 0; i < _countof(auxiliary_resources); ++i) {
            resources_.shader_manager->create_srv(
                auxiliary_resources[i]->get(),
                    eng::ResourceViewBuilder::build_srv(
                        auxiliary_resources[i]->get(),
                        eng::ResourceViewBuilder::EnumResourceType::ARRAY_2D),
                    SRVDescPos::DONUT_INDIRECT_DIFFUSE, i);
        }

        util::assure_next<
            SRVDescPos::DONUT_INDIRECT_DIFFUSE,
            SRVDescPos::DONUT_INDIRECT_SPECULAR,
            SRVDescPos::DONUT_SHADOW_CHANNELS,
            SRVDescPos::DONUT_AMBIENT_OCCLUSION>();

        resources_.shader_manager->create_uav(
            resources_.uav_output->get(),
            eng::ResourceViewBuilder::build_uav(
                resources_.uav_output->get(),
                eng::ResourceViewBuilder::EnumResourceType::ARRAY_2D,
                DXGI_FORMAT_R16G16B16A16_FLOAT),
            SRVDescPos::DONUT_HDR_COLOR_UAV);

        auto cs = dxutl::compile_shader(
            L"assets/shaders/donut/donut_deferred_lighting_CS.hlsl",
            "cs_5_1", "main", arguments
        );

        pso_.init(device);
        pso_.set_compute();
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
