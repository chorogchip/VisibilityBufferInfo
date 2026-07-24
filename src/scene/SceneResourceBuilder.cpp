#include "scene/SceneResourceBuilder.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstddef>
#include <cstring>
#include <limits>
#include <unordered_map>

#include "engine/MaterialGPU.h"
#include "engine/TextureLoader.h"
#include "util/Logger.h"
#include "util/Utils.h"
#include "dx_util/ResourceUtils.h"

#include "util/minmax_remover.h"

namespace scene {
    std::unique_ptr<SceneDataGPU> SceneResourceBuilder::build(
        const SceneDataCPU& src,
        ID3D12Device* p_device,
        ID3D12GraphicsCommandList* p_list,
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps,
        bool to_load_textures) {

        std::unique_ptr<SceneDataGPU> ret{ new SceneDataGPU{} };

        std::unordered_map<std::string, uint32_t> texture_indices;
        ret->materials.reserve(src.materials.size());

        std::vector<SceneDataGPU::MeshMetadata> meshes;
        meshes.reserve(src.meshes.size());
        for (const auto& mesh : src.meshes)
            meshes.push_back({ mesh.vertex_start, mesh.index_start });

        auto load_texture = [&](
            const eng::MaterialCPU::TexturePath& texture_path,
            bool srgb) -> uint32_t {
            if (!to_load_textures) {
                return eng::MaterialGPU::invalid_texture_index;
            }

            if (!texture_path || texture_path->empty()) {
                return eng::MaterialGPU::invalid_texture_index;
            }

            if (texture_path->native()[0] == L'*') {
                return eng::MaterialGPU::invalid_texture_index;
            }

            std::filesystem::path resolved_path = *texture_path;
            if (resolved_path.is_relative()) {
                resolved_path = src.source_path.parent_path() / resolved_path;
            }
            resolved_path = resolved_path.lexically_normal();

            const std::string cache_key = resolved_path.generic_string()
                + (srgb ? "|srgb" : "|linear");
            const auto cached = texture_indices.find(cache_key);
            if (cached != texture_indices.end()) return cached->second;

            eng::TextureLoadResult loaded = eng::TextureLoader::load(resolved_path);
            if (!loaded.succeeded()) {
                util::Logger::g_logger << loaded.error_message << '\n';
                return eng::MaterialGPU::invalid_texture_index;
            }

            if (loaded.metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D
                || DirectX::IsPlanar(loaded.metadata.format)) {
                util::Logger::g_logger
                    << "Unsupported texture dimension or planar format: "
                    << resolved_path.string() << '\n';
                return eng::MaterialGPU::invalid_texture_index;
            }

            util::Logger::g_logger.assert_with_log(
                loaded.metadata.arraySize <= std::numeric_limits<UINT16>::max()
                    && loaded.metadata.mipLevels <= std::numeric_limits<UINT16>::max(),
                "Texture array or mip count is too large");

            const DXGI_FORMAT texture_format = srgb
                ? DirectX::MakeSRGB(loaded.metadata.format)
                : DirectX::MakeLinear(loaded.metadata.format);

            D3D12_RESOURCE_DESC texture_desc{};
            texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            texture_desc.Width = loaded.metadata.width;
            texture_desc.Height = static_cast<UINT>(loaded.metadata.height);
            texture_desc.DepthOrArraySize = static_cast<UINT16>(loaded.metadata.arraySize);
            texture_desc.MipLevels = static_cast<UINT16>(loaded.metadata.mipLevels);
            texture_desc.Format = texture_format;
            texture_desc.SampleDesc.Count = 1;
            texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

            Microsoft::WRL::ComPtr<ID3D12Resource> texture =
                dxutl::create_committed_resource(
                    p_device, texture_desc, D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_STATE_COPY_DEST);

            const UINT subresource_count =
                static_cast<UINT>(loaded.metadata.arraySize * loaded.metadata.mipLevels);
            std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(subresource_count);
            std::vector<UINT> row_counts(subresource_count);
            std::vector<UINT64> row_sizes(subresource_count);
            UINT64 upload_size = 0;

            p_device->GetCopyableFootprints(
                &texture_desc, 0, subresource_count, 0,
                footprints.data(), row_counts.data(), row_sizes.data(), &upload_size);

            Microsoft::WRL::ComPtr<ID3D12Resource> upload =
                dxutl::create_buffer(
                    p_device, upload_size, D3D12_HEAP_TYPE_UPLOAD,
                    D3D12_RESOURCE_STATE_GENERIC_READ);

            auto* mapped = static_cast<std::byte*>(dxutl::map_upload_buffer(upload.Get()));
            for (size_t item = 0; item < loaded.metadata.arraySize; ++item) {
                for (size_t mip = 0; mip < loaded.metadata.mipLevels; ++mip) {
                    const UINT subresource = static_cast<UINT>(
                        item * loaded.metadata.mipLevels + mip);
                    const DirectX::Image* image = loaded.image.GetImage(mip, item, 0);
                    util::Logger::g_logger.assert_with_log(
                        image != nullptr, "Missing texture subresource");

                    std::byte* destination = mapped + footprints[subresource].Offset;
                    for (UINT row = 0; row < row_counts[subresource]; ++row) {
                        std::memcpy(
                            destination + static_cast<size_t>(row) * footprints[subresource].Footprint.RowPitch,
                            image->pixels + static_cast<size_t>(row) * image->rowPitch,
                            static_cast<size_t>(row_sizes[subresource]));
                    }
                }
            }
            upload->Unmap(0, nullptr);

            for (UINT subresource = 0; subresource < subresource_count; ++subresource) {
                D3D12_TEXTURE_COPY_LOCATION destination{};
                destination.pResource = texture.Get();
                destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                destination.SubresourceIndex = subresource;

                D3D12_TEXTURE_COPY_LOCATION source{};
                source.pResource = upload.Get();
                source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                source.PlacedFootprint = footprints[subresource];

                p_list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
            }

            dxutl::transition_resource(
                p_list, texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            const uint32_t texture_index = static_cast<uint32_t>(ret->textures.size());
            ret->textures.emplace_back(std::move(texture));
            used_upload_heaps.emplace_back(std::move(upload));
            texture_indices.emplace(cache_key, texture_index);
            return texture_index;
        };

        for (const auto& material : src.materials) {
            eng::MaterialGPU gpu_material{};
            gpu_material.base_color = material.base_color;
            gpu_material.emissive_color = material.emissive_color;
            gpu_material.emissive_intensity = material.emissive_intensity;
            gpu_material.metalness = material.metalness;
            gpu_material.roughness = material.roughness;
            gpu_material.opacity = material.opacity;
            gpu_material.alpha_cutoff = material.alpha_cutoff;
            gpu_material.normal_scale = material.normal_scale;
            gpu_material.occlusion_strength = material.occlusion_strength;

            gpu_material.base_color_texture = load_texture(material.base_color_texture, true);
            gpu_material.normal_texture = load_texture(material.normal_texture, false);
            gpu_material.metal_roughness_texture = load_texture(material.metal_roughness_texture, false);
            gpu_material.emissive_texture = load_texture(material.emissive_texture, true);
            gpu_material.occlusion_texture = load_texture(material.occlusion_texture, false);
            gpu_material.opacity_texture = load_texture(material.opacity_texture, false);

            gpu_material.flags =
                (gpu_material.base_color_texture != eng::MaterialGPU::invalid_texture_index ? 1u << 0 : 0)
                | (gpu_material.normal_texture != eng::MaterialGPU::invalid_texture_index ? 1u << 1 : 0)
                | (gpu_material.metal_roughness_texture != eng::MaterialGPU::invalid_texture_index ? 1u << 2 : 0)
                | (gpu_material.emissive_texture != eng::MaterialGPU::invalid_texture_index ? 1u << 3 : 0)
                | (gpu_material.occlusion_texture != eng::MaterialGPU::invalid_texture_index ? 1u << 4 : 0)
                | (gpu_material.opacity_texture != eng::MaterialGPU::invalid_texture_index ? 1u << 5 : 0);

            ret->materials.emplace_back(gpu_material);
        }

        constexpr size_t buffer_size_limit = std::numeric_limits<UINT>::max();

        util::Logger::g_logger.assert_with_log(
            src.vertices.size() > 0,
            "vertex buffer must not be empty");

        util::Logger::g_logger.assert_with_log_mul_overflow(
            src.vertices.size(), sizeof(decltype(src.vertices)::value_type), buffer_size_limit,
            "vertex buffer size overflow"
        );


        util::Logger::g_logger.assert_with_log(
            src.indices.size() > 0,
            "index buffer must not be empty");

        util::Logger::g_logger.assert_with_log_mul_overflow(
            src.indices.size(), sizeof(decltype(src.indices)::value_type), buffer_size_limit,
            "index buffer size overflow"
        );


        util::Logger::g_logger.assert_with_log(
            src.objects.size() > 0,
            "object buffer must not be empty");

        util::Logger::g_logger.assert_with_log_mul_overflow(
            src.objects.size(), sizeof(decltype(src.objects)::value_type), buffer_size_limit,
            "object buffer size overflow"
        );


        util::Logger::g_logger.assert_with_log(
            ret->materials.size() > 0,
            "material buffer must not be empty");

        util::Logger::g_logger.assert_with_log_mul_overflow(
            ret->materials.size(), sizeof(decltype(ret->materials)::value_type), buffer_size_limit,
            "material buffer size overflow"
        );

        util::Logger::g_logger.assert_with_log(
            meshes.size() > 0,
            "mesh buffer must not be empty");

        util::Logger::g_logger.assert_with_log_mul_overflow(
            meshes.size(), sizeof(decltype(meshes)::value_type), buffer_size_limit,
            "mesh buffer size overflow"
        );

        const size_t buf_sz_vertices = src.vertices.size() * sizeof(decltype(src.vertices)::value_type);
        const size_t buf_sz_indices = src.indices.size() * sizeof(decltype(src.indices)::value_type);
        const size_t buf_sz_objects = src.objects.size() * sizeof(decltype(src.objects)::value_type);
        const size_t buf_sz_materials = ret->materials.size() * sizeof(decltype(ret->materials)::value_type);
        const size_t buf_sz_meshes = meshes.size() * sizeof(decltype(meshes)::value_type);

        Microsoft::WRL::ComPtr<ID3D12Resource> upload_heap_vertex, upload_heap_index,
            upload_heap_object, upload_heap_material, upload_heap_mesh;

        upload_heap_vertex = dxutl::create_buffer(p_device, buf_sz_vertices, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        upload_heap_index = dxutl::create_buffer(p_device, buf_sz_indices, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        upload_heap_object = dxutl::create_buffer(p_device, buf_sz_objects, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        upload_heap_material = dxutl::create_buffer(p_device, buf_sz_materials, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        upload_heap_mesh = dxutl::create_buffer(p_device, buf_sz_meshes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        ret->vertex_buffer = dxutl::create_buffer(p_device, buf_sz_vertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
        ret->index_buffer = dxutl::create_buffer(p_device, buf_sz_indices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
        ret->object_buffer = dxutl::create_buffer(p_device, buf_sz_objects, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
        ret->material_buffer = dxutl::create_buffer(p_device, buf_sz_materials, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
        ret->mesh_buffer = dxutl::create_buffer(p_device, buf_sz_meshes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);

        dxutl::copy_to_upload_buffer(upload_heap_vertex.Get(), src.vertices.data(), buf_sz_vertices);
        dxutl::copy_to_upload_buffer(upload_heap_index.Get(), src.indices.data(), buf_sz_indices);
        dxutl::copy_to_upload_buffer(upload_heap_object.Get(), src.objects.data(), buf_sz_objects);
        dxutl::copy_to_upload_buffer(upload_heap_material.Get(), ret->materials.data(), buf_sz_materials);
        dxutl::copy_to_upload_buffer(upload_heap_mesh.Get(), meshes.data(), buf_sz_meshes);

        p_list->CopyBufferRegion(ret->vertex_buffer.Get(), 0, upload_heap_vertex.Get(), 0, buf_sz_vertices);
        p_list->CopyBufferRegion(ret->index_buffer.Get(), 0, upload_heap_index.Get(), 0, buf_sz_indices);
        p_list->CopyBufferRegion(ret->object_buffer.Get(), 0, upload_heap_object.Get(), 0, buf_sz_objects);
        p_list->CopyBufferRegion(ret->material_buffer.Get(), 0, upload_heap_material.Get(), 0, buf_sz_materials);
        p_list->CopyBufferRegion(ret->mesh_buffer.Get(), 0, upload_heap_mesh.Get(), 0, buf_sz_meshes);

        // COMMON -> COPY_DEST: implicit transition

        dxutl::transition_resource(p_list, ret->vertex_buffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dxutl::transition_resource(p_list, ret->index_buffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER |
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dxutl::transition_resource(p_list, ret->object_buffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        dxutl::transition_resource(p_list, ret->material_buffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dxutl::transition_resource(p_list, ret->mesh_buffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        used_upload_heaps.emplace_back(std::move(upload_heap_vertex));
        used_upload_heaps.emplace_back(std::move(upload_heap_index));
        used_upload_heaps.emplace_back(std::move(upload_heap_object));
        used_upload_heaps.emplace_back(std::move(upload_heap_material));
        used_upload_heaps.emplace_back(std::move(upload_heap_mesh));

        ret->vertex_buffer_view.BufferLocation = ret->vertex_buffer->GetGPUVirtualAddress();
        ret->vertex_buffer_view.StrideInBytes = sizeof(SceneDataCPU::Vertex);
        ret->vertex_buffer_view.SizeInBytes = static_cast<UINT>(buf_sz_vertices);

        ret->index_buffer_view.BufferLocation = ret->index_buffer->GetGPUVirtualAddress();
        ret->index_buffer_view.Format = DXGI_FORMAT_R32_UINT; static_assert(sizeof(SceneDataCPU::Index) == 4);
        ret->index_buffer_view.SizeInBytes = static_cast<UINT>(buf_sz_indices);

        return ret;
    }
}
