#include "render/pass/donut/PassDonutVisUtil.h"

#include <algorithm>
#include <cstdint>
#include <limits>

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "engine/ResourceManagerShader.h"
#include "engine/ResourceViewBuilder.h"
#include "engine/RootSignatureBuilder.h"
#include "util/Logger.h"

namespace rndr {

    namespace {

        enum class BinRootParam : UINT {
            DISPATCH_CONSTANTS,
            VISIBILITY,
            GEOMETRY_INSTANCE_BUFFER,
            SUBMESH_BUFFER,
            MATERIAL_BUFFER,
            BIN_PREFIX,
            BIN_COUNTS,
            PIXEL_LIST,
        };

        enum class PrefixRootParam : UINT {
            DISPATCH_CONSTANTS,
            SOURCE,
            DESTINATION,
            DISPATCH,
        };

        enum class ClearRootParam : UINT {
            BIN_COUNTS,
        };

        struct DispatchConstants {
            std::uint32_t width = 0;
            std::uint32_t height = 0;
        };
    }

    void PassDonutVisUtil::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutVisUtilResources& resources) {

        resources_ = resources;

        util::Logger::g_logger.assert_with_log(
            device != nullptr &&
            resources_.shader_manager != nullptr &&
            resources_.visibility_buf != nullptr &&
            resources_.scene != nullptr &&
            resources_.pixel_list != nullptr &&
            resources_.indirect_dispatch_list != nullptr,
            "Donut visibility util pass requires complete resources");

        for (const scene::DonutSceneGPUData::MaterialData& material :
            resources_.scene->material_data) {

            util::Logger::g_logger.assert_with_log(
                material.virtual_shader_id <= MAX_REAL_SHADER_ID,
                "Donut material virtual shader id exceeds visutil bin limit");
        }

        const UINT bin_byte_size =
            MAX_SHADER_COUNT * sizeof(std::uint32_t);
        bin_counts_.init(
            dxutl::create_uav_buffer(
                device,
                bin_byte_size,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        bin_prefix_.init(
            dxutl::create_uav_buffer(
                device,
                bin_byte_size,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS).Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        resources_.shader_manager->create_srv(
            resources_.visibility_buf->get(),
            eng::ResourceViewBuilder::build_srv(
                resources_.visibility_buf->get(),
                eng::ResourceViewBuilder::EnumResourceType::TEXTURE_2D,
                DXGI_FORMAT_R32G32_UINT),
            eng::ResourceManagerShader::EnumDescPos::DONUT_VISIBILITY_BUFFER);

        auto cs_clear = dxutl::compile_shader(
            L"assets/shaders/visutil/material_binning_CS.hlsl",
            L"cs_6_5",
            L"kernel_clear_counts",
            arguments);
        auto cs_binning = dxutl::compile_shader(
            L"assets/shaders/visutil/material_binning_CS.hlsl",
            L"cs_6_5",
            L"kernel_material_binning",
            arguments);
        auto cs_prefix = dxutl::compile_shader(
            L"assets/shaders/visutil/prefix_scan.CS.hlsl",
            L"cs_6_5",
            L"kernel_prefix_block",
            arguments);
        auto cs_flatten = dxutl::compile_shader(
            L"assets/shaders/visutil/material_flatten_CS.hlsl",
            L"cs_6_5",
            L"kernel_material_flatten",
            arguments);

        auto clear_root_signature = eng::RootSignatureBuilder{}
            .root_uav().reg(0).add()  // BIN_COUNTS
            .build(device);

        auto bin_root_signature = eng::RootSignatureBuilder{}
            .constant().reg(0).cnt(2).add()  // DISPATCH_CONSTANTS
            .srv_tabl().reg(0).cnt(1).add()  // VISIBILITY
            .root_srv().reg(1).add()         // GEOMETRY_INSTANCE_BUFFER
            .root_srv().reg(2).add()         // SUBMESH_BUFFER
            .root_srv().reg(3).add()         // MATERIAL_BUFFER
            .root_srv().reg(4).add()         // BIN_PREFIX
            .root_uav().reg(0).add()         // BIN_COUNTS
            .root_uav().reg(1).add()         // PIXEL_LIST
            .root_uav().reg(2).add()         // DISPATCH
            .build(device);

        auto prefix_root_signature = eng::RootSignatureBuilder{}
            .constant().reg(0).cnt(1).add()  // DISPATCH_CONSTANTS
            .root_srv().reg(0).add()         // SOURCE
            .root_uav().reg(0).add()         // DESTINATION
            .build(device);

        pso_clear_counts_.init(device);
        pso_clear_counts_.set_compute();
        pso_clear_counts_.set_root_signature(clear_root_signature.Get());
        pso_clear_counts_.set_shader_compute(cs_clear.Get());
        pso_clear_counts_.build();

        pso_binning_.init(device);
        pso_binning_.set_compute();
        pso_binning_.set_root_signature(bin_root_signature.Get());
        pso_binning_.set_shader_compute(cs_binning.Get());
        pso_binning_.build();

        pso_prefixscan_.init(device);
        pso_prefixscan_.set_compute();
        pso_prefixscan_.set_root_signature(prefix_root_signature.Get());
        pso_prefixscan_.set_shader_compute(cs_prefix.Get());
        pso_prefixscan_.build();

        pso_flatten_.init(device);
        pso_flatten_.set_compute();
        pso_flatten_.set_root_signature(bin_root_signature.Get());
        pso_flatten_.set_shader_compute(cs_flatten.Get());
        pso_flatten_.build();
    }

    void PassDonutVisUtil::render(
        ID3D12GraphicsCommandList* command_list,
        UINT width,
        UINT height) {

        const std::uint64_t pixel_count =
            static_cast<std::uint64_t>(width) * height;
        util::Logger::g_logger.assert_with_log(
            command_list != nullptr &&
            width > 0 &&
            height > 0 &&
            pixel_count <= (std::numeric_limits<std::uint32_t>::max)(),
            "Donut visibility util dispatch dimensions are invalid");

        resources_.visibility_buf->transition(
            command_list,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        bin_counts_.transition(
            command_list,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        ID3D12DescriptorHeap* heaps[] = { resources_.shader_manager->get() };
        command_list->SetDescriptorHeaps(_countof(heaps), heaps);

        command_list->SetPipelineState(pso_clear_counts_.get());
        command_list->SetComputeRootSignature(
            pso_clear_counts_.get_root_signature());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(ClearRootParam::BIN_COUNTS),
            bin_counts_.get()->GetGPUVirtualAddress());
        command_list->Dispatch(1, 1, 1);
        bin_counts_.uav_barrier(command_list);

        const DispatchConstants constants{ width, height };
        const UINT group_x = (width + 15) / 16;
        const UINT group_y = (height + 15) / 16;

        command_list->SetPipelineState(pso_binning_.get());
        command_list->SetComputeRootSignature(
            pso_binning_.get_root_signature());
        command_list->SetComputeRoot32BitConstants(
            static_cast<UINT>(BinRootParam::DISPATCH_CONSTANTS),
            sizeof(DispatchConstants) / sizeof(std::uint32_t),
            &constants,
            0);
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(BinRootParam::VISIBILITY),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_VISIBILITY_BUFFER));
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(BinRootParam::GEOMETRY_INSTANCE_BUFFER),
            resources_.scene->geometry_instance_buffer.get()->
                GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(BinRootParam::SUBMESH_BUFFER),
            resources_.scene->submesh_buffer.get()->GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(BinRootParam::MATERIAL_BUFFER),
            resources_.scene->material_buffer.get()->GetGPUVirtualAddress());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(BinRootParam::BIN_COUNTS),
            bin_counts_.get()->GetGPUVirtualAddress());
        command_list->Dispatch(group_x, group_y, 1);
        bin_counts_.uav_barrier(command_list);

        bin_counts_.transition(
            command_list,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        bin_prefix_.transition(
            command_list,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        resources_.indirect_dispatch_list->transition(
            command_list,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        const std::uint32_t bin_count = MAX_SHADER_COUNT;
        command_list->SetPipelineState(pso_prefixscan_.get());
        command_list->SetComputeRootSignature(
            pso_prefixscan_.get_root_signature());
        command_list->SetComputeRoot32BitConstants(
            static_cast<UINT>(PrefixRootParam::DISPATCH_CONSTANTS),
            1,
            &bin_count,
            0);
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(PrefixRootParam::SOURCE),
            bin_counts_.get()->GetGPUVirtualAddress());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(PrefixRootParam::DESTINATION),
            bin_prefix_.get()->GetGPUVirtualAddress());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(PrefixRootParam::DISPATCH),
            resources_.indirect_dispatch_list->get()->GetGPUVirtualAddress());
        command_list->Dispatch(1, 1, 1);
        bin_prefix_.uav_barrier(command_list);
        resources_.indirect_dispatch_list->uav_barrier(command_list);

        bin_counts_.transition(
            command_list,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        command_list->SetPipelineState(pso_clear_counts_.get());
        command_list->SetComputeRootSignature(
            pso_clear_counts_.get_root_signature());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(ClearRootParam::BIN_COUNTS),
            bin_counts_.get()->GetGPUVirtualAddress());
        command_list->Dispatch(1, 1, 1);
        bin_counts_.uav_barrier(command_list);

        bin_prefix_.transition(
            command_list,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        resources_.pixel_list->transition(
            command_list,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        command_list->SetPipelineState(pso_flatten_.get());
        command_list->SetComputeRootSignature(
            pso_flatten_.get_root_signature());
        command_list->SetComputeRoot32BitConstants(
            static_cast<UINT>(BinRootParam::DISPATCH_CONSTANTS),
            sizeof(DispatchConstants) / sizeof(std::uint32_t),
            &constants,
            0);
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(BinRootParam::VISIBILITY),
            resources_.shader_manager->get_gpu_adr(
                eng::ResourceManagerShader::EnumDescPos::DONUT_VISIBILITY_BUFFER));
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(BinRootParam::GEOMETRY_INSTANCE_BUFFER),
            resources_.scene->geometry_instance_buffer.get()->
                GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(BinRootParam::SUBMESH_BUFFER),
            resources_.scene->submesh_buffer.get()->GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(BinRootParam::MATERIAL_BUFFER),
            resources_.scene->material_buffer.get()->GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(BinRootParam::BIN_PREFIX),
            bin_prefix_.get()->GetGPUVirtualAddress());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(BinRootParam::BIN_COUNTS),
            bin_counts_.get()->GetGPUVirtualAddress());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(BinRootParam::PIXEL_LIST),
            resources_.pixel_list->get()->GetGPUVirtualAddress());
        command_list->Dispatch(group_x, group_y, 1);

        bin_counts_.uav_barrier(command_list);
        resources_.pixel_list->uav_barrier(command_list);
    }
}
