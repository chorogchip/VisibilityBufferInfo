#include "render/renderer/benchmark/RendererBenchmark.h"

#include <cstring>
#include <limits>

#include "util/DummyTextureGen.h"
#include "util/RenderConstants.h"
#include "util/Utils.h"
#include "util/BenchmarkCsvWriter.h"
#include "dx_util/ResourceUtils.h"
#include "engine/ResourceManagerShader.h"
#include "scene/SceneFingerprint.h"
#include "scene/SceneLoader.h"
#include "scene/SceneResourceBuilder.h"

namespace rndr {

    void RendererBenchmark::init1_() {

        const UINT texture_begin = static_cast<UINT>(
            eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_TEXTURE_BEGIN);

        resource_manager_frame_.init(device_.Get());
        depth_stencil_buffer_.init(
            dxutl::create_depth_stencil_buffer(
                device_.Get(),
                width_,
                height_,
                util::RenderConstants::DEPTH_STENCIL_FORMAT,
                D3D12_RESOURCE_STATE_DEPTH_WRITE
            ).Get(),
            D3D12_RESOURCE_STATE_DEPTH_WRITE);

        for (UINT i = 0; i < util::Constants::FRAME_COUNT; ++i)
            buf_constant_[i].init(device_.Get());

        scene_cpu_ = scene::SceneLoader::load(program_argument_);

        scene::SceneFingerprint::write_csv(
            util::get_scene_fingerprint_output_path(program_argument_.output_filepath),
            *scene_cpu_,
            program_argument_);

        {
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> used_upload_heaps;

            util::Utils::throw_if_failed(command_list_->Reset(
                command_allocator_[frame_index_].Get(), nullptr));

            scene_gpu_ = scene::SceneResourceBuilder::build(
                *scene_cpu_, device_.Get(), command_list_.Get(), used_upload_heaps,
                program_argument_.to_load_texture);

            util::Utils::throw_if_failed(command_list_->Close(),
                "close list on resource creation");

            graphics_queue_.execute(command_list_.Get());
            graphics_queue_.wait_idle();
        }

        this->wrap_scene_resources();

        if (program_argument_.to_load_texture)
            program_argument_.texture_count = static_cast<UINT>(scene_gpu_->textures.size());

        resource_manager_sampler_.init(device_.Get());
        resource_manager_shader_.init(device_.Get(),
            texture_begin + program_argument_.texture_count);

        this->create_dummy_textures();

        resource_manager_sampler_.create_sampler(
            eng::ResourceManagerSampler::EnumDescPos::BENCH_MATERIAL,
            eng::ResourceManagerSampler::EnumSamplerType::LINEAR_WRAP);

        this->init2_();
    }

    void RendererBenchmark::render_prepare_() {

        auto& cam_buf = buf_constant_[frame_index_];

        DirectX::XMStoreFloat4x4(
            &cam_buf.buffer.mat_view_,
            DirectX::XMMatrixTranspose(camera_.get_mat_view()));

        DirectX::XMStoreFloat4x4(
            &cam_buf.buffer.mat_proj_,
            DirectX::XMMatrixTranspose(camera_.get_mat_proj(width_, height_)));

        cam_buf.buffer.viewport_size_ = DirectX::XMFLOAT2(
            static_cast<float>(width_), static_cast<float>(height_));

        cam_buf.update();

    }

    void RendererBenchmark::wrap_scene_resources() {
        util::Logger::g_logger.assert_with_log(
            scene_gpu_ != nullptr, "benchmark scene GPU data must be initialized");

        scene_vertex_buffer_.init(
            scene_gpu_->vertex_buffer.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        scene_index_buffer_.init(
            scene_gpu_->index_buffer.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        scene_object_buffer_.init(
            scene_gpu_->object_buffer.Get(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        scene_material_buffer_.init(
            scene_gpu_->material_buffer.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        scene_mesh_buffer_.init(
            scene_gpu_->mesh_buffer.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    void RendererBenchmark::create_dummy_textures() {

        textures_.clear();

        if (program_argument_.to_load_texture) {
            textures_.reserve(scene_gpu_->textures.size());
            for (const auto& texture : scene_gpu_->textures) {
                textures_.emplace_back();
                textures_.back().init(
                    texture.Get(),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
            return;
        }

        const UINT texture_count = program_argument_.texture_count;
        const UINT texture_size = program_argument_.texture_size;

        util::Logger::g_logger.assert_with_log(
            texture_count > 0,
            "texture count must be greater than zero");
        util::Logger::g_logger.assert_with_log(
            texture_size > 0,
            "texture size must be greater than zero");

        textures_.clear();
        textures_.resize(texture_count);

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
        util::Utils::throw_if_failed(command_list_->Reset(
            command_allocator_[frame_index_].Get(), nullptr));

        for (UINT texture_index = 0; texture_index < texture_count; ++texture_index) {
            const auto texture_data = util::create_dummy_texture_data(
                texture_size, texture_size, texture_index);

            auto texture = dxutl::create_committed_resource(
                device_.Get(), texture_desc, D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COPY_DEST);
            textures_[texture_index].init(
                texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
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
            destination.pResource = textures_[texture_index].get();
            destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

            D3D12_TEXTURE_COPY_LOCATION source{};
            source.pResource = upload_buffers[texture_index].Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source.PlacedFootprint = footprint;

            command_list_->CopyTextureRegion(
                &destination, 0, 0, 0, &source, nullptr);
            textures_[texture_index].transition(
                command_list_.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        util::Utils::throw_if_failed(command_list_->Close(),
            "close list on dummy texture creation");
        graphics_queue_.execute(command_list_.Get());
        graphics_queue_.wait_idle();
    }
}
