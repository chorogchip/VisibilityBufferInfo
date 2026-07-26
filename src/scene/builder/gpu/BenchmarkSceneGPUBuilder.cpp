#include "scene/builder/gpu/BenchmarkSceneGPUBuilder.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <DirectXTex.h>

#include "dx_util/ResourceUtils.h"
#include "engine/TextureLoader.h"
#include "util/Logger.h"

namespace scene {

    namespace {

        void upload_buffer(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            const void* source,
            size_t byte_size,
            D3D12_RESOURCE_STATES final_state,
            eng::GPUResource& destination,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
                used_upload_heaps) {
            util::Logger::g_logger.assert_with_log(
                device != nullptr &&
                command_list != nullptr &&
                source != nullptr &&
                byte_size > 0,
                "GPU scene buffer upload requires valid non-empty inputs.");

            Microsoft::WRL::ComPtr<ID3D12Resource> upload =
                dxutl::create_buffer(
                    device,
                    static_cast<UINT64>(byte_size),
                    D3D12_HEAP_TYPE_UPLOAD,
                    D3D12_RESOURCE_STATE_GENERIC_READ);
            Microsoft::WRL::ComPtr<ID3D12Resource> resource =
                dxutl::create_buffer(
                    device,
                    static_cast<UINT64>(byte_size),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_STATE_COPY_DEST);

            dxutl::copy_to_upload_buffer(
                upload.Get(),
                source,
                byte_size);
            command_list->CopyBufferRegion(
                resource.Get(),
                0,
                upload.Get(),
                0,
                byte_size);
            destination.init(
                resource.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST);
            destination.transition(command_list, final_state);
            used_upload_heaps.emplace_back(std::move(upload));
        }

        DirectX::XMFLOAT3X4 to_shader_transform(
            const DirectX::XMFLOAT4X4& transform) {
            return {
                transform._11, transform._21, transform._31, transform._41,
                transform._12, transform._22, transform._32, transform._42,
                transform._13, transform._23, transform._33, transform._43
            };
        }

        uint32_t load_texture(
            const SceneCPUData::Material::TexturePath& path,
            bool srgb,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            BenchmarkSceneGPUData& destination,
            std::unordered_map<std::string, uint32_t>& texture_cache,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
                used_upload_heaps,
            bool load_textures) {
            if (!load_textures || !path) {
                return source::SceneConstants::INVALID_INDEX;
            }

            const std::string cache_key =
                path->generic_string() + (srgb ? "|srgb" : "|linear");
            const auto cached = texture_cache.find(cache_key);
            if (cached != texture_cache.end()) {
                return cached->second;
            }

            eng::TextureLoadResult loaded = eng::TextureLoader::load(*path);
            if (!loaded.succeeded()) {
                util::Logger::g_logger << loaded.error_message << '\n';
                return source::SceneConstants::INVALID_INDEX;
            }
            if (loaded.metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D ||
                DirectX::IsPlanar(loaded.metadata.format)) {
                util::Logger::g_logger
                    << "Unsupported scene texture: "
                    << path->string()
                    << '\n';
                return source::SceneConstants::INVALID_INDEX;
            }

            util::Logger::g_logger.assert_with_log(
                loaded.metadata.arraySize <=
                (std::numeric_limits<UINT16>::max)() &&
                loaded.metadata.mipLevels <=
                (std::numeric_limits<UINT16>::max)() &&
                loaded.metadata.height <=
                (std::numeric_limits<UINT>::max)(),
                "GPU scene texture dimensions exceed D3D12 limits.");

            D3D12_RESOURCE_DESC texture_desc{};
            texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            texture_desc.Width = loaded.metadata.width;
            texture_desc.Height = static_cast<UINT>(loaded.metadata.height);
            texture_desc.DepthOrArraySize =
                static_cast<UINT16>(loaded.metadata.arraySize);
            texture_desc.MipLevels =
                static_cast<UINT16>(loaded.metadata.mipLevels);
            texture_desc.Format = srgb
                ? DirectX::MakeSRGB(loaded.metadata.format)
                : DirectX::MakeLinear(loaded.metadata.format);
            texture_desc.SampleDesc.Count = 1;
            texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

            Microsoft::WRL::ComPtr<ID3D12Resource> texture =
                dxutl::create_committed_resource(
                    device,
                    texture_desc,
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_STATE_COPY_DEST);

            const UINT subresource_count = static_cast<UINT>(
                loaded.metadata.arraySize * loaded.metadata.mipLevels);
            std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(
                subresource_count);
            std::vector<UINT> row_counts(subresource_count);
            std::vector<UINT64> row_sizes(subresource_count);
            UINT64 upload_size = 0;
            device->GetCopyableFootprints(
                &texture_desc,
                0,
                subresource_count,
                0,
                footprints.data(),
                row_counts.data(),
                row_sizes.data(),
                &upload_size);

            Microsoft::WRL::ComPtr<ID3D12Resource> upload =
                dxutl::create_buffer(
                    device,
                    upload_size,
                    D3D12_HEAP_TYPE_UPLOAD,
                    D3D12_RESOURCE_STATE_GENERIC_READ);
            std::byte* mapped = static_cast<std::byte*>(
                dxutl::map_upload_buffer(upload.Get()));

            for (size_t item = 0; item < loaded.metadata.arraySize; ++item) {
                for (size_t mip = 0; mip < loaded.metadata.mipLevels; ++mip) {
                    const UINT subresource = static_cast<UINT>(
                        item * loaded.metadata.mipLevels + mip);
                    const DirectX::Image* image =
                        loaded.image.GetImage(mip, item, 0);
                    util::Logger::g_logger.assert_with_log(
                        image != nullptr,
                        "GPU scene texture is missing a subresource.");

                    std::byte* target =
                        mapped + footprints[subresource].Offset;
                    for (UINT row = 0; row < row_counts[subresource]; ++row) {
                        std::memcpy(
                            target +
                            static_cast<size_t>(row) *
                            footprints[subresource].Footprint.RowPitch,
                            image->pixels +
                            static_cast<size_t>(row) * image->rowPitch,
                            static_cast<size_t>(row_sizes[subresource]));
                    }
                }
            }
            upload->Unmap(0, nullptr);

            for (UINT subresource = 0; subresource < subresource_count; ++subresource) {
                D3D12_TEXTURE_COPY_LOCATION target{};
                target.pResource = texture.Get();
                target.Type =
                    D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                target.SubresourceIndex = subresource;

                D3D12_TEXTURE_COPY_LOCATION source{};
                source.pResource = upload.Get();
                source.Type =
                    D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                source.PlacedFootprint = footprints[subresource];
                command_list->CopyTextureRegion(
                    &target,
                    0,
                    0,
                    0,
                    &source,
                    nullptr);
            }

            eng::GPUResource gpu_texture{};
            gpu_texture.init(
                texture.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST);
            gpu_texture.transition(
                command_list,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

            util::Logger::g_logger.assert_with_log(
                destination.textures.size() <
                (std::numeric_limits<uint32_t>::max)(),
                "GPU scene texture count exceeds 32-bit indexing.");
            const uint32_t texture_id =
                static_cast<uint32_t>(destination.textures.size());
            destination.textures.emplace_back(std::move(gpu_texture));
            used_upload_heaps.emplace_back(std::move(upload));
            texture_cache.emplace(cache_key, texture_id);
            return texture_id;
        }
    }

    BenchmarkSceneGPUData BenchmarkSceneGPUBuilder::build(
        const SceneCPUData& source,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
            used_upload_heaps,
        bool load_textures) {
        source.validate();
        util::Logger::g_logger.assert_with_log(
            device != nullptr && command_list != nullptr,
            "GPU scene build requires a device and command list.");
        util::Logger::g_logger.assert_with_log(
            source.vertices.size() <=
            (std::numeric_limits<uint32_t>::max)() &&
            source.indices.size() <=
            (std::numeric_limits<uint32_t>::max)() &&
            source.meshes.size() <=
            (std::numeric_limits<uint32_t>::max)() &&
            source.submeshes.size() <=
            (std::numeric_limits<uint32_t>::max)() &&
            source.materials.size() <=
            (std::numeric_limits<uint32_t>::max)() &&
            source.instances.size() <=
            (std::numeric_limits<uint32_t>::max)() &&
            source.draw_instance_ids.size() <=
            (std::numeric_limits<uint32_t>::max)() &&
            source.draw_calls.size() <=
            (std::numeric_limits<uint32_t>::max)(),
            "GPU scene data exceeds 32-bit indexing.");

        BenchmarkSceneGPUData destination{};
        std::vector<BenchmarkSceneGPUData::MeshData> meshes;
        std::vector<BenchmarkSceneGPUData::SubmeshData> submeshes;
        std::vector<BenchmarkSceneGPUData::MaterialData> materials;
        std::vector<BenchmarkSceneGPUData::InstanceData> instances;
        std::vector<BenchmarkSceneGPUData::InstanceData> render_instances;
        std::vector<BenchmarkSceneGPUData::DrawData> draws;
        std::unordered_map<std::string, uint32_t> texture_cache;

        meshes.reserve(source.meshes.size());
        for (const SceneCPUData::Mesh& mesh : source.meshes) {
            meshes.push_back({
                mesh.first_submesh,
                mesh.submesh_count,
                0,
                0
            });
        }

        submeshes.reserve(source.submeshes.size());
        for (const SceneCPUData::Submesh& submesh : source.submeshes) {
            submeshes.push_back({
                submesh.vertex_offset,
                submesh.vertex_count,
                submesh.index_offset,
                submesh.index_count,
                submesh.material_id,
                0,
                0,
                0
            });
        }

        materials.reserve(source.materials.size());
        for (const SceneCPUData::Material& material : source.materials) {
            BenchmarkSceneGPUData::MaterialData gpu_material{};
            gpu_material.base_color = material.base_color;
            gpu_material.emissive_color = material.emissive_color;
            gpu_material.emissive_intensity =
                material.emissive_intensity;
            gpu_material.metalness = material.metalness;
            gpu_material.roughness = material.roughness;
            gpu_material.opacity = material.opacity;
            gpu_material.alpha_cutoff = material.alpha_cutoff;
            gpu_material.normal_scale = material.normal_scale;
            gpu_material.occlusion_strength =
                material.occlusion_strength;
            if (material.alpha_tested) {
                gpu_material.flags |=
                    BenchmarkSceneGPUData::MATERIAL_FLAG_ALPHA_TESTED;
            }
            if (material.double_sided) {
                gpu_material.flags |=
                    BenchmarkSceneGPUData::MATERIAL_FLAG_DOUBLE_SIDED;
            }

            const SceneCPUData::Material::TexturePath texture_paths[] = {
                material.base_color_texture,
                material.metal_roughness_texture,
                material.normal_texture,
                material.emissive_texture,
                material.occlusion_texture
            };
            constexpr bool texture_srgb[] = {
                true,
                false,
                false,
                true,
                false
            };
            constexpr uint32_t texture_flags[] = {
                BenchmarkSceneGPUData::MATERIAL_FLAG_BASE_COLOR_TEXTURE,
                BenchmarkSceneGPUData::MATERIAL_FLAG_METAL_ROUGHNESS_TEXTURE,
                BenchmarkSceneGPUData::MATERIAL_FLAG_NORMAL_TEXTURE,
                BenchmarkSceneGPUData::MATERIAL_FLAG_EMISSIVE_TEXTURE,
                BenchmarkSceneGPUData::MATERIAL_FLAG_OCCLUSION_TEXTURE
            };

            for (size_t slot = 0; slot < gpu_material.texture_indices.size(); ++slot) {
                gpu_material.texture_indices[slot] = load_texture(
                    texture_paths[slot],
                    texture_srgb[slot],
                    device,
                    command_list,
                    destination,
                    texture_cache,
                    used_upload_heaps,
                    load_textures);
                if (gpu_material.texture_indices[slot] !=
                    source::SceneConstants::INVALID_INDEX) {
                    gpu_material.flags |= texture_flags[slot];
                }
            }
            materials.emplace_back(gpu_material);
        }

        instances.reserve(source.instances.size());
        for (uint32_t instance_id = 0; instance_id < source.instances.size(); ++instance_id) {
            const SceneCPUData::Instance& instance =
                source.instances[instance_id];
            instances.push_back({
                to_shader_transform(instance.world_transform),
                instance_id,
                instance.mesh_id,
                0,
                0
            });
        }

        render_instances.reserve(source.draw_instance_ids.size());
        for (uint32_t instance_id : source.draw_instance_ids) {
            render_instances.emplace_back(instances[instance_id]);
        }

        draws.reserve(source.draw_calls.size());
        for (const SceneCPUData::DrawCall& draw : source.draw_calls) {
            draws.push_back({
                draw.first_instance,
                draw.instance_count,
                draw.submesh_id,
                draw.index_count,
                draw.index_offset,
                draw.vertex_offset,
                draw.material_id,
                0
            });
        }

        upload_buffer(
            device,
            command_list,
            source.vertices.data(),
            source.vertices.size() * sizeof(SceneCPUData::Vertex),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.vertex_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            source.indices.data(),
            source.indices.size() * sizeof(uint32_t),
            D3D12_RESOURCE_STATE_INDEX_BUFFER |
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.index_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            meshes.data(),
            meshes.size() * sizeof(BenchmarkSceneGPUData::MeshData),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.mesh_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            submeshes.data(),
            submeshes.size() * sizeof(BenchmarkSceneGPUData::SubmeshData),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.submesh_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            materials.data(),
            materials.size() * sizeof(BenchmarkSceneGPUData::MaterialData),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.material_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            instances.data(),
            instances.size() * sizeof(BenchmarkSceneGPUData::InstanceData),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.instance_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            render_instances.data(),
            render_instances.size() * sizeof(BenchmarkSceneGPUData::InstanceData),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.render_instance_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            draws.data(),
            draws.size() * sizeof(BenchmarkSceneGPUData::DrawData),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.draw_buffer,
            used_upload_heaps);

        util::Logger::g_logger.assert_with_log(
            source.vertices.size() * sizeof(SceneCPUData::Vertex) <=
            (std::numeric_limits<UINT>::max)() &&
            source.indices.size() * sizeof(uint32_t) <=
            (std::numeric_limits<UINT>::max)(),
            "GPU scene input-assembler views exceed UINT size.");

        destination.vertex_buffer_view.BufferLocation =
            destination.vertex_buffer.get()->GetGPUVirtualAddress();
        destination.vertex_buffer_view.SizeInBytes = static_cast<UINT>(
            source.vertices.size() * sizeof(SceneCPUData::Vertex));
        destination.vertex_buffer_view.StrideInBytes =
            sizeof(SceneCPUData::Vertex);
        destination.index_buffer_view.BufferLocation =
            destination.index_buffer.get()->GetGPUVirtualAddress();
        destination.index_buffer_view.SizeInBytes = static_cast<UINT>(
            source.indices.size() * sizeof(uint32_t));
        destination.index_buffer_view.Format = DXGI_FORMAT_R32_UINT;

        destination.draw_calls = source.draw_calls;
        destination.vertex_count =
            static_cast<uint32_t>(source.vertices.size());
        destination.index_count =
            static_cast<uint32_t>(source.indices.size());
        destination.mesh_count =
            static_cast<uint32_t>(source.meshes.size());
        destination.submesh_count =
            static_cast<uint32_t>(source.submeshes.size());
        destination.material_count =
            static_cast<uint32_t>(source.materials.size());
        destination.instance_count =
            static_cast<uint32_t>(source.instances.size());
        destination.render_instance_count =
            static_cast<uint32_t>(render_instances.size());
        destination.draw_count =
            static_cast<uint32_t>(source.draw_calls.size());
        destination.validate();
        return destination;
    }
}
