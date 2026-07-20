#include "render/renderer/benchmark/RendererBenchmark.h"

#include <cstring>
#include <limits>

#include "dx_util/ResourceUtils.h"
#include "engine/ResourceManagerShader.h"
#include "scene/SceneFingerprint.h"
#include "scene/SceneLoader.h"
#include "scene/SceneResourceBuilder.h"
#include "util/BenchmarkCsvWriter.h"
#include "util/DummyTextureGen.h"
#include "util/Utils.h"

namespace rndr {

    void RendererBenchmark::init_scene_() {
        load_scene_();
        create_scene_resources_();

        benchmark_program_arguments_ = *program_arguments_;
        benchmark_program_arguments_.texture_count = material_texture_count_();
    }

    void RendererBenchmark::init_shader_resources_() {
        const UINT texture_count = material_texture_count_();
        const UINT texture_begin = static_cast<UINT>(
            eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_TEXTURE_BEGIN);

        util::Logger::g_logger.assert_with_log(
            texture_count > 0,
            "benchmark material texture count must be greater than zero");
        util::Logger::g_logger.assert_with_log(
            texture_begin <= (std::numeric_limits<UINT>::max)() - texture_count,
            "benchmark shader descriptor count overflow");

        resource_manager_shader_.init(device_.Get(), texture_begin + texture_count);
    }

    void RendererBenchmark::init_renderer_resources_() {
        create_benchmark_resources_();
        create_sampler_descriptor_();
    }

    const std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
        RendererBenchmark::material_textures() const {
        if (program_arguments_->to_load_texture)
            return scene_gpu_->textures;
        return dummy_textures_;
    }

    void RendererBenchmark::load_scene_() {
        scene_cpu_ = scene::SceneLoader::load(*program_arguments_);
        scene::SceneFingerprint::write_csv(
            util::get_scene_fingerprint_output_path(program_arguments_->output_filepath),
            *scene_cpu_,
            *program_arguments_);
    }

    void RendererBenchmark::create_scene_resources_() {
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> used_upload_heaps;
        Utils::throw_if_failed(command_list_->Reset(
            command_allocator_[frame_index_].Get(), nullptr));

        scene_gpu_ = scene::SceneResourceBuilder::build(
            *scene_cpu_, device_.Get(), command_list_.Get(), used_upload_heaps,
            program_arguments_->to_load_texture);

        Utils::throw_if_failed(command_list_->Close(),
            "close list on resource creation");

        graphics_queue_.execute(command_list_.Get());
        graphics_queue_.wait_idle();
    }

    void RendererBenchmark::create_benchmark_resources_() {
        if (program_arguments_->to_load_texture) {
            dummy_textures_.clear();
            return;
        }

        const UINT texture_count = program_arguments_->texture_count;
        const UINT texture_size = program_arguments_->texture_size;

        util::Logger::g_logger.assert_with_log(
            texture_count > 0,
            "texture count must be greater than zero");
        util::Logger::g_logger.assert_with_log(
            texture_size > 0,
            "texture size must be greater than zero");

        dummy_textures_.clear();
        dummy_textures_.resize(texture_count);

        D3D12_RESOURCE_DESC texture_desc{};
        texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texture_desc.Width = texture_size;
        texture_desc.Height = texture_size;
        texture_desc.DepthOrArraySize = 1;
        texture_desc.MipLevels = 1;
        texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        UINT64 upload_size = 0;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT row_count = 0;
        UINT64 row_size_in_bytes = 0;
        device_->GetCopyableFootprints(
            &texture_desc, 0, 1, 0, &footprint,
            &row_count, &row_size_in_bytes, &upload_size);

        (void)row_count;
        (void)row_size_in_bytes;

        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> upload_buffers(texture_count);
        Utils::throw_if_failed(command_list_->Reset(
            command_allocator_[frame_index_].Get(), nullptr));

        for (UINT texture_index = 0; texture_index < texture_count; ++texture_index) {
            const auto texture_data = util::create_dummy_texture_data(
                texture_size, texture_size, texture_index);

            dummy_textures_[texture_index] = dxutl::create_committed_resource(
                device_.Get(), texture_desc, D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COPY_DEST);
            upload_buffers[texture_index] = dxutl::create_buffer(
                device_.Get(), upload_size, D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_STATE_GENERIC_READ);

            void* mapped_data = dxutl::map_upload_buffer(upload_buffers[texture_index].Get());
            const std::size_t source_row_pitch = static_cast<std::size_t>(texture_size) * 4u;
            auto* destination_data = static_cast<unsigned char*>(mapped_data);

            for (UINT y = 0; y < texture_size; ++y) {
                auto* destination_row = destination_data +
                    footprint.Offset +
                    static_cast<std::size_t>(y) * footprint.Footprint.RowPitch;
                const auto* source_row = texture_data.data() +
                    static_cast<std::size_t>(y) * source_row_pitch;
                std::memcpy(destination_row, source_row, source_row_pitch);
            }
            upload_buffers[texture_index]->Unmap(0, nullptr);

            D3D12_TEXTURE_COPY_LOCATION destination{};
            destination.pResource = dummy_textures_[texture_index].Get();
            destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

            D3D12_TEXTURE_COPY_LOCATION source{};
            source.pResource = upload_buffers[texture_index].Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source.PlacedFootprint = footprint;

            command_list_->CopyTextureRegion(
                &destination, 0, 0, 0, &source, nullptr);
            dxutl::transition_resource(
                command_list_.Get(), dummy_textures_[texture_index].Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        Utils::throw_if_failed(command_list_->Close(),
            "close list on dummy texture creation");
        graphics_queue_.execute(command_list_.Get());
        graphics_queue_.wait_idle();
    }

    void RendererBenchmark::create_sampler_descriptor_() {
        D3D12_SAMPLER_DESC sampler_desc{};
        sampler_desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler_desc.MaxAnisotropy = 1;
        sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;
        resource_manager_sampler_.create_sampler(
            eng::ResourceManagerSampler::EnumDescPos::BENCH_MATERIAL,
            sampler_desc);
    }

    UINT RendererBenchmark::material_texture_count_() const {
        if (!program_arguments_->to_load_texture)
            return program_arguments_->texture_count;

        util::Logger::g_logger.assert_with_log(
            scene_gpu_ != nullptr,
            "scene GPU resources must exist before resolving material textures");
        util::Logger::g_logger.assert_with_log(
            !scene_gpu_->textures.empty(),
            "to_load_texture requested, but the scene has no loadable textures");
        util::Logger::g_logger.assert_with_log(
            scene_gpu_->textures.size() <= (std::numeric_limits<UINT>::max)(),
            "scene texture count exceeds UINT");
        return static_cast<UINT>(scene_gpu_->textures.size());
    }
}
