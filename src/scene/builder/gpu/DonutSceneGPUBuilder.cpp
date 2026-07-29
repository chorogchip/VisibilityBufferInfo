#include "scene/builder/gpu/DonutSceneGPUBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <DirectXTex.h>

#include "dx_util/ResourceUtils.h"
#include "engine/TextureLoader.h"
#include "scene/builder/cpu/SceneCPUValidator.h"
#include "scene/builder/gpu/DonutSceneGPUValidator.h"
#include "util/Logger.h"

namespace scene {

    namespace {

        constexpr size_t BASE_COLOR_TEXTURE = 0;
        constexpr size_t METAL_ROUGHNESS_TEXTURE = 1;
        constexpr size_t NORMAL_TEXTURE = 2;
        constexpr size_t EMISSIVE_TEXTURE = 3;
        constexpr size_t OCCLUSION_TEXTURE = 4;
        constexpr size_t TRANSMISSION_TEXTURE = 5;
        constexpr size_t OPACITY_TEXTURE = 6;
        constexpr std::uint64_t TEXTURE_UPLOAD_BATCH_BYTES =
            256ull * 1024ull * 1024ull;

        size_t align_16(size_t value) {
            return (value + 15u) & ~size_t(15u);
        }

        size_t align_constant_buffer(size_t value) {
            return (
                value +
                D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT -
                1u) &
                ~size_t(
                    D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT -
                    1u);
        }

        uint32_t to_uint32(size_t value, const char* message) {
            util::Logger::g_logger.assert_with_log(
                value <= (std::numeric_limits<uint32_t>::max)(),
                message);
            return static_cast<uint32_t>(value);
        }

        UINT to_uint(size_t value, const char* message) {
            util::Logger::g_logger.assert_with_log(
                value <= (std::numeric_limits<UINT>::max)(),
                message);
            return static_cast<UINT>(value);
        }

        template <typename T>
        void append_vertex_stream(
            std::vector<std::byte>& destination,
            const std::vector<T>& source,
            uint32_t& offset) {

            const size_t stream_offset = align_16(destination.size());
            const size_t stream_size = source.size() * sizeof(T);
            offset = to_uint32(
                stream_offset,
                "Donut vertex stream offset exceeds 32-bit addressing.");
            destination.resize(stream_offset + stream_size);
            std::memcpy(
                destination.data() + stream_offset,
                source.data(),
                stream_size);
        }

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
                "Donut GPU buffer upload requires valid non-empty inputs.");

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

        uint32_t pack_snorm8(float x, float y, float z, float w) {
            const auto pack = [](float value) {
                const int32_t converted = static_cast<int32_t>(
                    std::round(std::clamp(value, -1.0f, 1.0f) * 127.0f));
                return static_cast<uint32_t>(converted & 0xff);
            };
            return pack(x) |
                (pack(y) << 8) |
                (pack(z) << 16) |
                (pack(w) << 24);
        }

        uint32_t create_fallback_texture(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            DonutSceneGPUData& destination,
            const std::array<uint8_t, 4>& pixel,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
                used_upload_heaps) {

            D3D12_RESOURCE_DESC texture_desc{};
            texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            texture_desc.Width = 1;
            texture_desc.Height = 1;
            texture_desc.DepthOrArraySize = 1;
            texture_desc.MipLevels = 1;
            texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texture_desc.SampleDesc.Count = 1;
            texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

            Microsoft::WRL::ComPtr<ID3D12Resource> texture =
                dxutl::create_committed_resource(
                    device,
                    texture_desc,
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_STATE_COPY_DEST);

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
            UINT row_count = 0;
            UINT64 row_size = 0;
            UINT64 upload_size = 0;
            device->GetCopyableFootprints(
                &texture_desc,
                0,
                1,
                0,
                &footprint,
                &row_count,
                &row_size,
                &upload_size);
            (void)row_count;
            (void)row_size;

            Microsoft::WRL::ComPtr<ID3D12Resource> upload =
                dxutl::create_buffer(
                    device,
                    upload_size,
                    D3D12_HEAP_TYPE_UPLOAD,
                    D3D12_RESOURCE_STATE_GENERIC_READ);
            std::byte* mapped = static_cast<std::byte*>(
                dxutl::map_upload_buffer(upload.Get()));
            std::memcpy(
                mapped + footprint.Offset,
                pixel.data(),
                pixel.size());
            upload->Unmap(0, nullptr);

            D3D12_TEXTURE_COPY_LOCATION source{};
            source.pResource = upload.Get();
            source.Type =
                D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source.PlacedFootprint = footprint;

            D3D12_TEXTURE_COPY_LOCATION target{};
            target.pResource = texture.Get();
            target.Type =
                D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            target.SubresourceIndex = 0;
            command_list->CopyTextureRegion(
                &target,
                0,
                0,
                0,
                &source,
                nullptr);

            eng::GPUResource gpu_texture{};
            gpu_texture.init(
                texture.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST);
            gpu_texture.transition(
                command_list,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

            util::Logger::g_logger.assert_with_log(
                destination.textures.size() <
                DonutSceneGPUData::
                    MAX_MATERIAL_TEXTURE_DESCRIPTOR_COUNT,
                "Donut texture count exceeds the descriptor limit.");
            const uint32_t texture_id =
                static_cast<uint32_t>(destination.textures.size());
            destination.textures.emplace_back(std::move(gpu_texture));
            used_upload_heaps.emplace_back(std::move(upload));
            return texture_id;
        }

        std::optional<uint32_t> load_texture(
            const SceneCPUData::Material::TexturePath& path,
            bool srgb,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            DonutSceneGPUData& destination,
            std::unordered_map<std::string, uint32_t>& cache,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
                used_upload_heaps,
            const std::function<void()>& flush_uploads,
            std::uint64_t& pending_texture_upload_bytes,
            bool load_textures) {

            if (!load_textures || !path) return std::nullopt;

            const std::string cache_key =
                path->generic_string() +
                (srgb ? "|srgb" : "|linear");
            const auto cached = cache.find(cache_key);
            if (cached != cache.end()) return cached->second;

            eng::TextureLoadResult loaded =
                eng::TextureLoader::load(*path);
            if (!loaded.succeeded()) {
                util::Logger::g_logger
                    << loaded.error_message
                    << '\n';
                return std::nullopt;
            }
            if (loaded.metadata.dimension !=
                DirectX::TEX_DIMENSION_TEXTURE2D ||
                DirectX::IsPlanar(loaded.metadata.format) ||
                loaded.metadata.arraySize != 1 ||
                loaded.metadata.mipLevels >
                (std::numeric_limits<UINT16>::max)() ||
                loaded.metadata.height >
                (std::numeric_limits<UINT>::max)()) {
                util::Logger::g_logger
                    << "Unsupported Donut material texture: "
                    << path->string()
                    << '\n';
                return std::nullopt;
            }

            D3D12_RESOURCE_DESC texture_desc{};
            texture_desc.Dimension =
                D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            texture_desc.Width = loaded.metadata.width;
            texture_desc.Height =
                static_cast<UINT>(loaded.metadata.height);
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

            const UINT subresource_count = to_uint(
                loaded.metadata.arraySize *
                loaded.metadata.mipLevels,
                "Donut texture has too many subresources.");
            std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>
                footprints(subresource_count);
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
                    const UINT subresource =
                        static_cast<UINT>(
                            item *
                            loaded.metadata.mipLevels +
                            mip);
                    const DirectX::Image* image =
                        loaded.image.GetImage(mip, item, 0);
                    util::Logger::g_logger.assert_with_log(
                        image != nullptr,
                        "Donut texture has a missing subresource.");

                    std::byte* target =
                        mapped + footprints[subresource].Offset;
                    for (UINT row = 0; row < row_counts[subresource]; ++row) {
                        std::memcpy(
                            target +
                            static_cast<size_t>(row) *
                            footprints[subresource].
                                Footprint.RowPitch,
                            image->pixels +
                            static_cast<size_t>(row) *
                            image->rowPitch,
                            static_cast<size_t>(
                                row_sizes[subresource]));
                    }
                }
            }
            upload->Unmap(0, nullptr);

            for (UINT subresource = 0; subresource < subresource_count; ++subresource) {
                D3D12_TEXTURE_COPY_LOCATION source{};
                source.pResource = upload.Get();
                source.Type =
                    D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                source.PlacedFootprint = footprints[subresource];

                D3D12_TEXTURE_COPY_LOCATION target{};
                target.pResource = texture.Get();
                target.Type =
                    D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                target.SubresourceIndex = subresource;
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
                DonutSceneGPUData::
                    MAX_MATERIAL_TEXTURE_DESCRIPTOR_COUNT,
                "Donut texture count exceeds the descriptor limit.");
            const uint32_t texture_id =
                static_cast<uint32_t>(destination.textures.size());
            destination.textures.emplace_back(std::move(gpu_texture));
            used_upload_heaps.emplace_back(std::move(upload));
            pending_texture_upload_bytes += upload_size;
            if (flush_uploads &&
                pending_texture_upload_bytes >= TEXTURE_UPLOAD_BATCH_BYTES) {
                flush_uploads();
                pending_texture_upload_bytes = 0;
            }
            cache.emplace(cache_key, texture_id);
            return texture_id;
        }

        DonutSceneGPUData::ShaderMaterialConstants
            build_material_constants(
                const DonutSceneGPUData::MaterialData& source,
                uint32_t material_id) {

            DonutSceneGPUData::ShaderMaterialConstants constants{};
            constants.base_or_diffuse_color = {
                source.base_color.x,
                source.base_color.y,
                source.base_color.z
            };
            constants.material_id =
                static_cast<int32_t>(material_id);
            constants.emissive_color = source.emissive_color;
            constants.domain = source.domain;
            constants.opacity = source.base_color.w;
            constants.roughness = source.roughness;
            constants.metalness = source.metalness;
            constants.normal_texture_scale = source.normal_scale;
            constants.occlusion_strength =
                source.occlusion_strength;
            constants.alpha_cutoff = source.alpha_cutoff;
            constants.normal_texture_transform_scale =
                { 1.0f, 1.0f };

            if ((source.flags &
                DonutSceneGPUData::
                    MATERIAL_FLAG_DOUBLE_SIDED) != 0) {
                constants.flags |=
                    DonutSceneGPUData::
                        SHADER_MATERIAL_FLAG_DOUBLE_SIDED;
            }
            if ((source.flags &
                DonutSceneGPUData::
                    MATERIAL_FLAG_BASE_COLOR_TEXTURE) != 0) {
                constants.flags |=
                    DonutSceneGPUData::
                        SHADER_MATERIAL_FLAG_USE_BASE_COLOR_TEXTURE;
            }
            if ((source.flags &
                DonutSceneGPUData::
                    MATERIAL_FLAG_METAL_ROUGHNESS_TEXTURE) != 0) {
                constants.flags |=
                    DonutSceneGPUData::
                        SHADER_MATERIAL_FLAG_USE_METAL_ROUGHNESS_TEXTURE;
            }
            if ((source.flags &
                DonutSceneGPUData::
                    MATERIAL_FLAG_NORMAL_TEXTURE) != 0) {
                constants.flags |=
                    DonutSceneGPUData::
                        SHADER_MATERIAL_FLAG_USE_NORMAL_TEXTURE;
            }
            if ((source.flags &
                DonutSceneGPUData::
                    MATERIAL_FLAG_EMISSIVE_TEXTURE) != 0) {
                constants.flags |=
                    DonutSceneGPUData::
                        SHADER_MATERIAL_FLAG_USE_EMISSIVE_TEXTURE;
            }
            if ((source.flags &
                DonutSceneGPUData::
                    MATERIAL_FLAG_OCCLUSION_TEXTURE) != 0) {
                constants.flags |=
                    DonutSceneGPUData::
                        SHADER_MATERIAL_FLAG_USE_OCCLUSION_TEXTURE;
            }

            constants.base_or_diffuse_texture_index =
                static_cast<int32_t>(
                    source.texture_indices[
                        BASE_COLOR_TEXTURE]);
            constants.metal_rough_or_specular_texture_index =
                static_cast<int32_t>(
                    source.texture_indices[
                        METAL_ROUGHNESS_TEXTURE]);
            constants.normal_texture_index =
                static_cast<int32_t>(
                    source.texture_indices[
                        NORMAL_TEXTURE]);
            constants.emissive_texture_index =
                static_cast<int32_t>(
                    source.texture_indices[
                        EMISSIVE_TEXTURE]);
            constants.occlusion_texture_index =
                static_cast<int32_t>(
                    source.texture_indices[
                        OCCLUSION_TEXTURE]);
            constants.transmission_texture_index =
                static_cast<int32_t>(
                    source.texture_indices[
                        TRANSMISSION_TEXTURE]);
            constants.opacity_texture_index =
                static_cast<int32_t>(
                    source.texture_indices[
                        OPACITY_TEXTURE]);
            return constants;
        }
    }

    DonutSceneGPUData DonutSceneGPUBuilder::build(
        const SceneCPUData& source,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
            used_upload_heaps,
        const std::function<void()>& flush_uploads,
        bool load_textures) {

        SceneCPUValidator::validate(source);
        util::Logger::g_logger.assert_with_log(
            device != nullptr && command_list != nullptr,
            "Donut GPU scene build requires a device and command list.");
        util::Logger::g_logger.assert_with_log(
            source.vertices.size() <=
            (std::numeric_limits<uint32_t>::max)() &&
            source.indices.size() <=
            (std::numeric_limits<uint32_t>::max)() &&
            source.submeshes.size() <=
            (std::numeric_limits<uint32_t>::max)() &&
            source.instances.size() <=
            (std::numeric_limits<uint32_t>::max)() &&
            source.draw_instances.size() <=
            (std::numeric_limits<uint32_t>::max)(),
            "Donut GPU scene exceeds 32-bit indexing.");

        DonutSceneGPUData destination{};

        std::vector<DirectX::XMFLOAT3> positions;
        std::vector<DirectX::XMFLOAT3> prev_positions;
        std::vector<DirectX::XMFLOAT2> texcoords;
        std::vector<uint32_t> packed_normals;
        std::vector<uint32_t> packed_tangents;
        positions.reserve(source.vertices.size());
        prev_positions.reserve(source.vertices.size());
        texcoords.reserve(source.vertices.size());
        packed_normals.reserve(source.vertices.size());
        packed_tangents.reserve(source.vertices.size());
        for (const SceneCPUData::Vertex& vertex : source.vertices) {
            positions.push_back(vertex.position);
            prev_positions.push_back(vertex.position);
            texcoords.push_back(vertex.uv0);
            packed_normals.push_back(pack_snorm8(
                vertex.normal.x,
                vertex.normal.y,
                vertex.normal.z,
                0.0f));
            packed_tangents.push_back(pack_snorm8(
                vertex.tangent.x,
                vertex.tangent.y,
                vertex.tangent.z,
                vertex.tangent.w));
        }

        std::vector<std::byte> vertex_data;
        append_vertex_stream(
            vertex_data,
            positions,
            destination.vertex_layout.position_offset);
        append_vertex_stream(
            vertex_data,
            prev_positions,
            destination.vertex_layout.prev_position_offset);
        append_vertex_stream(
            vertex_data,
            texcoords,
            destination.vertex_layout.texcoord_offset);
        append_vertex_stream(
            vertex_data,
            packed_normals,
            destination.vertex_layout.normal_offset);
        append_vertex_stream(
            vertex_data,
            packed_tangents,
            destination.vertex_layout.tangent_offset);
        destination.vertex_layout.byte_size = to_uint32(
            vertex_data.size(),
            "Donut vertex buffer exceeds 32-bit addressing.");

        destination.submesh_data.reserve(source.submeshes.size());
        for (const SceneCPUData::Submesh& source_submesh : source.submeshes) {
            destination.submesh_data.push_back({
                source_submesh.vertex_offset,
                source_submesh.vertex_count,
                source_submesh.index_offset,
                source_submesh.index_count,
                source_submesh.material_id,
                0,
                0,
                0
            });
        }

        destination.instance_data.reserve(source.instances.size());
        for (uint32_t instance_id = 0; instance_id < source.instances.size(); ++instance_id) {
            const SceneCPUData::Instance& source_instance =
                source.instances[instance_id];
            const SceneCPUData::Mesh& source_mesh =
                source.meshes[source_instance.mesh_id];

            DonutSceneGPUData::InstanceData instance{};
            instance.first_geometry_instance =
                static_cast<uint32_t>(
                    destination.geometry_instance_data.size());
            instance.first_geometry = source_mesh.first_submesh;
            instance.geometry_instance_count =
                source_mesh.submesh_count;
            instance.transform =
                to_shader_transform(source_instance.world_transform);
            instance.prev_transform = instance.transform;
            destination.instance_data.push_back(instance);

            const uint32_t submesh_end =
                source_mesh.first_submesh +
                source_mesh.submesh_count;
            for (uint32_t submesh_id = source_mesh.first_submesh; submesh_id < submesh_end; ++submesh_id) {
                destination.geometry_instance_data.push_back({
                    instance_id,
                    submesh_id,
                    0,
                    0
                });
            }
        }

        destination.draw_instance_data.reserve(
            source.draw_instances.size());
        std::vector<uint32_t> draw_instance_ids;
        draw_instance_ids.reserve(source.draw_instances.size());
        for (uint32_t draw_instance_id = 0;
            draw_instance_id < source.draw_instances.size();
            ++draw_instance_id) {
            const SceneCPUData::DrawInstance& source_draw_instance =
                source.draw_instances[draw_instance_id];
            destination.draw_instance_data.push_back({
                source_draw_instance.instance_id,
                source_draw_instance.submesh_id
            });
            draw_instance_ids.push_back(draw_instance_id);
        }

        destination.fallback_texture_indices[0] =
            create_fallback_texture(
                device,
                command_list,
                destination,
                { 255, 255, 255, 255 },
                used_upload_heaps);
        destination.fallback_texture_indices[1] =
            create_fallback_texture(
                device,
                command_list,
                destination,
                { 0, 0, 0, 255 },
                used_upload_heaps);
        destination.fallback_texture_indices[2] =
            create_fallback_texture(
                device,
                command_list,
                destination,
                { 128, 128, 255, 255 },
                used_upload_heaps);

        std::unordered_map<std::string, uint32_t> texture_cache;
        std::uint64_t pending_texture_upload_bytes = 0;
        destination.material_data.reserve(source.materials.size());
        for (const SceneCPUData::Material& source_material : source.materials) {
            DonutSceneGPUData::MaterialData material{};
            material.base_color = source_material.base_color;
            material.emissive_color = {
                source_material.emissive_color.x *
                source_material.emissive_intensity,
                source_material.emissive_color.y *
                source_material.emissive_intensity,
                source_material.emissive_color.z *
                source_material.emissive_intensity
            };
            material.roughness = source_material.roughness;
            material.metalness = source_material.metalness;
            material.normal_scale = source_material.normal_scale;
            material.occlusion_strength =
                source_material.occlusion_strength;
            material.alpha_cutoff = source_material.alpha_cutoff;
            material.virtual_shader_id = source_material.virtual_shader_id;
            if (source_material.double_sided) {
                material.flags |=
                    DonutSceneGPUData::
                        MATERIAL_FLAG_DOUBLE_SIDED;
            }
            if (source_material.alpha_mode != source::AlphaMode::Opaque) {
                material.flags |=
                    DonutSceneGPUData::
                        MATERIAL_FLAG_ALPHA_TESTED;
                material.domain =
                    DonutSceneGPUData::
                        SHADER_MATERIAL_DOMAIN_ALPHA_TESTED;
            }

            const SceneCPUData::Material::TexturePath paths[] = {
                source_material.base_color_texture,
                source_material.metal_roughness_texture,
                source_material.normal_texture,
                source_material.emissive_texture,
                source_material.occlusion_texture
            };
            constexpr bool SRGB[] = {
                true,
                false,
                false,
                true,
                false
            };
            constexpr uint32_t FLAGS[] = {
                DonutSceneGPUData::
                    MATERIAL_FLAG_BASE_COLOR_TEXTURE,
                DonutSceneGPUData::
                    MATERIAL_FLAG_METAL_ROUGHNESS_TEXTURE,
                DonutSceneGPUData::
                    MATERIAL_FLAG_NORMAL_TEXTURE,
                DonutSceneGPUData::
                    MATERIAL_FLAG_EMISSIVE_TEXTURE,
                DonutSceneGPUData::
                    MATERIAL_FLAG_OCCLUSION_TEXTURE
            };
            const uint32_t fallback_ids[] = {
                destination.fallback_texture_indices[0],
                destination.fallback_texture_indices[0],
                destination.fallback_texture_indices[2],
                destination.fallback_texture_indices[1],
                destination.fallback_texture_indices[0]
            };

            for (size_t slot = 0; slot < std::size(paths); ++slot) {
                const std::optional<uint32_t> texture_id =
                    load_texture(
                        paths[slot],
                        SRGB[slot],
                        device,
                        command_list,
                        destination,
                        texture_cache,
                        used_upload_heaps,
                        flush_uploads,
                        pending_texture_upload_bytes,
                        load_textures);
                material.texture_indices[slot] =
                    texture_id
                    ? *texture_id
                    : fallback_ids[slot];
                if (texture_id) {
                    material.flags |= FLAGS[slot];
                }
            }
            material.texture_indices[TRANSMISSION_TEXTURE] =
                destination.fallback_texture_indices[0];
            material.texture_indices[OPACITY_TEXTURE] =
                destination.fallback_texture_indices[0];
            destination.material_data.push_back(material);
        }

        std::vector<uint32_t> indices = source.indices;
        for (const SceneCPUData::Submesh& submesh : source.submeshes) {
            const uint32_t index_end =
                submesh.index_offset + submesh.index_count;
            for (uint32_t index_id = submesh.index_offset; index_id < index_end; ++index_id) {
                indices[index_id] += submesh.vertex_offset;
            }
        }

        destination.material_constant_stride = to_uint32(
            align_constant_buffer(
                sizeof(
                    DonutSceneGPUData::
                        ShaderMaterialConstants)),
            "Donut material constant stride exceeds 32-bit addressing.");
        const size_t material_constant_byte_size =
            static_cast<size_t>(
                destination.material_constant_stride) *
            destination.material_data.size();
        std::vector<std::byte> material_constants(
            material_constant_byte_size);
        for (size_t material_id = 0; material_id < destination.material_data.size(); ++material_id) {
            const DonutSceneGPUData::ShaderMaterialConstants constants =
                build_material_constants(
                    destination.material_data[material_id],
                    static_cast<uint32_t>(material_id));
            std::memcpy(
                material_constants.data() +
                material_id *
                destination.material_constant_stride,
                &constants,
                sizeof(constants));
        }

        upload_buffer(
            device,
            command_list,
            vertex_data.data(),
            vertex_data.size(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.vertex_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            indices.data(),
            indices.size() * sizeof(uint32_t),
            D3D12_RESOURCE_STATE_INDEX_BUFFER |
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.index_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            destination.instance_data.data(),
            destination.instance_data.size() *
            sizeof(DonutSceneGPUData::InstanceData),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.instance_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            destination.draw_instance_data.data(),
            destination.draw_instance_data.size() *
            sizeof(DonutSceneGPUData::DrawInstanceData),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.draw_instance_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            draw_instance_ids.data(),
            draw_instance_ids.size() * sizeof(uint32_t),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.draw_instance_id_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            destination.submesh_data.data(),
            destination.submesh_data.size() *
            sizeof(DonutSceneGPUData::SubmeshData),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.submesh_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            destination.geometry_instance_data.data(),
            destination.geometry_instance_data.size() *
            sizeof(
                DonutSceneGPUData::
                    GeometryInstanceData),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.geometry_instance_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            destination.material_data.data(),
            destination.material_data.size() *
            sizeof(DonutSceneGPUData::MaterialData),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination.material_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            material_constants.data(),
            material_constants.size(),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            destination.material_constant_buffer,
            used_upload_heaps);

        destination.vertex_count =
            static_cast<uint32_t>(source.vertices.size());
        destination.index_count =
            static_cast<uint32_t>(indices.size());
        destination.index_buffer_view.BufferLocation =
            destination.index_buffer.get()->GetGPUVirtualAddress();
        destination.index_buffer_view.SizeInBytes = to_uint(
            indices.size() * sizeof(uint32_t),
            "Donut index buffer exceeds the D3D12 view limit.");
        destination.index_buffer_view.Format =
            DXGI_FORMAT_R32_UINT;
        destination.draw_instance_id_capacity =
            static_cast<uint32_t>(draw_instance_ids.size());


        destination.active_material_class_count = source.active_material_class_count;


        DonutSceneGPUValidator::validate(destination);
        return destination;
    }
}
