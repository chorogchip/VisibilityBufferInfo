#include "scene/donut/DonutSceneResourceBuilder.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <DirectXTex.h>

#include "dx_util/ResourceUtils.h"
#include "engine/TextureLoader.h"
#include "util/Logger.h"

namespace scene::donut {

    namespace {

        constexpr uint32_t fallback_texture_resource = 0;

        size_t align_16(size_t value) {
            return (value + 15u) & ~size_t(15u);
        }

        uint32_t to_uint32(size_t value, const char* message) {
            util::Logger::g_logger.assert_with_log(
                value <= (std::numeric_limits<uint32_t>::max)(), message);
            return static_cast<uint32_t>(value);
        }

        uint32_t to_uint(size_t value, const char* message) {
            util::Logger::g_logger.assert_with_log(
                value <= (std::numeric_limits<UINT>::max)(), message);
            return static_cast<uint32_t>(value);
        }

        template <typename T>
        void append_vertex_stream(
            std::vector<std::byte>& destination,
            const std::vector<T>& source,
            uint32_t& offset) {
            const size_t stream_offset = align_16(destination.size());
            util::Logger::g_logger.assert_with_log(
                source.size() <= (std::numeric_limits<size_t>::max)() / sizeof(T),
                "Donut vertex stream size overflow");
            const size_t stream_size = source.size() * sizeof(T);
            util::Logger::g_logger.assert_with_log(
                stream_offset <= (std::numeric_limits<size_t>::max)() - stream_size,
                "Donut vertex buffer size overflow");
            offset = to_uint32(stream_offset, "Donut vertex offset exceeds 32-bit addressing");
            destination.resize(stream_offset + stream_size);
            std::memcpy(destination.data() + stream_offset, source.data(), stream_size);
        }

        void upload_buffer(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            const void* source,
            size_t byte_size,
            D3D12_RESOURCE_STATES final_state,
            Microsoft::WRL::ComPtr<ID3D12Resource>& destination,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps) {
            util::Logger::g_logger.assert_with_log(
                byte_size > 0, "Donut GPU buffer must not be empty");

            Microsoft::WRL::ComPtr<ID3D12Resource> upload = dxutl::create_buffer(
                device, static_cast<UINT64>(byte_size), D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_STATE_GENERIC_READ);
            destination = dxutl::create_buffer(
                device, static_cast<UINT64>(byte_size), D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COMMON);

            dxutl::copy_to_upload_buffer(upload.Get(), source, byte_size);
            command_list->CopyBufferRegion(destination.Get(), 0, upload.Get(), 0, byte_size);
            dxutl::transition_resource(
                command_list, destination.Get(), D3D12_RESOURCE_STATE_COPY_DEST, final_state);
            used_upload_heaps.emplace_back(std::move(upload));
        }

        int32_t texture_flag(MaterialTextureSlot slot) {
            switch (slot) {
            case MaterialTextureSlot::base_or_diffuse:
                return material_flag_use_base_or_diffuse;
            case MaterialTextureSlot::metal_rough_or_specular:
                return material_flag_use_metal_rough_or_specular;
            case MaterialTextureSlot::normal:
                return material_flag_use_normal;
            case MaterialTextureSlot::emissive:
                return material_flag_use_emissive;
            case MaterialTextureSlot::occlusion:
                return material_flag_use_occlusion;
            case MaterialTextureSlot::transmission:
                return material_flag_use_transmission;
            case MaterialTextureSlot::opacity:
                return material_flag_use_opacity;
            case MaterialTextureSlot::count:
                break;
            }
            return 0;
        }

        bool texture_is_srgb(MaterialTextureSlot slot) {
            return slot == MaterialTextureSlot::base_or_diffuse ||
                slot == MaterialTextureSlot::emissive;
        }

        uint32_t create_fallback_texture(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            DonutSceneDataGPU& destination,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps) {
            const std::array<uint8_t, 4> pixel = {255, 255, 255, 255};

            D3D12_RESOURCE_DESC texture_desc{};
            texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            texture_desc.Width = 1;
            texture_desc.Height = 1;
            texture_desc.DepthOrArraySize = 1;
            texture_desc.MipLevels = 1;
            texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texture_desc.SampleDesc.Count = 1;
            texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

            auto texture = dxutl::create_committed_resource(
                device, texture_desc, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
            UINT row_count = 0;
            UINT64 row_size = 0;
            UINT64 upload_size = 0;
            device->GetCopyableFootprints(
                &texture_desc, 0, 1, 0, &footprint, &row_count, &row_size, &upload_size);

            auto upload = dxutl::create_buffer(
                device, upload_size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
            auto* mapped = static_cast<std::byte*>(dxutl::map_upload_buffer(upload.Get()));
            std::memcpy(mapped + footprint.Offset, pixel.data(), pixel.size());
            upload->Unmap(0, nullptr);

            D3D12_TEXTURE_COPY_LOCATION source{};
            source.pResource = upload.Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source.PlacedFootprint = footprint;

            D3D12_TEXTURE_COPY_LOCATION target{};
            target.pResource = texture.Get();
            target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            target.SubresourceIndex = 0;
            command_list->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);
            dxutl::transition_resource(
                command_list, texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

            const uint32_t texture_index = static_cast<uint32_t>(destination.textures.size());
            destination.textures.emplace_back(std::move(texture));
            used_upload_heaps.emplace_back(std::move(upload));
            return texture_index;
        }

        std::optional<uint32_t> load_texture(
            const DonutSceneDataCPU& source,
            const DonutSceneDataCPU::TexturePath& source_path,
            bool srgb,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            DonutSceneDataGPU& destination,
            std::unordered_map<std::string, uint32_t>& cache,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps) {
            if (!source_path || source_path->empty() || source_path->native()[0] == L'*') {
                return std::nullopt;
            }

            std::filesystem::path resolved_path = *source_path;
            if (resolved_path.is_relative()) {
                resolved_path = source.source_path.parent_path() / resolved_path;
            }
            resolved_path = resolved_path.lexically_normal();

            const std::string cache_key = resolved_path.generic_string() +
                (srgb ? "|srgb" : "|linear");
            const auto cached = cache.find(cache_key);
            if (cached != cache.end()) {
                return cached->second;
            }

            eng::TextureLoadResult loaded = eng::TextureLoader::load(resolved_path);
            if (!loaded.succeeded()) {
                util::Logger::g_logger << loaded.error_message << '\n';
                return std::nullopt;
            }

            if (loaded.metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D ||
                DirectX::IsPlanar(loaded.metadata.format) ||
                loaded.metadata.arraySize > (std::numeric_limits<UINT16>::max)() ||
                loaded.metadata.mipLevels > (std::numeric_limits<UINT16>::max)() ||
                loaded.metadata.height > (std::numeric_limits<UINT>::max)()) {
                util::Logger::g_logger << "Unsupported Donut material texture: "
                    << resolved_path.string() << '\n';
                return std::nullopt;
            }

            D3D12_RESOURCE_DESC texture_desc{};
            texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            texture_desc.Width = loaded.metadata.width;
            texture_desc.Height = static_cast<UINT>(loaded.metadata.height);
            texture_desc.DepthOrArraySize = static_cast<UINT16>(loaded.metadata.arraySize);
            texture_desc.MipLevels = static_cast<UINT16>(loaded.metadata.mipLevels);
            texture_desc.Format = srgb
                ? DirectX::MakeSRGB(loaded.metadata.format)
                : DirectX::MakeLinear(loaded.metadata.format);
            texture_desc.SampleDesc.Count = 1;
            texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

            auto texture = dxutl::create_committed_resource(
                device, texture_desc, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);

            const size_t subresource_count_size = loaded.metadata.arraySize * loaded.metadata.mipLevels;
            const UINT subresource_count = to_uint(
                subresource_count_size, "Donut texture has too many subresources");
            std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(subresource_count);
            std::vector<UINT> row_counts(subresource_count);
            std::vector<UINT64> row_sizes(subresource_count);
            UINT64 upload_size = 0;
            device->GetCopyableFootprints(
                &texture_desc, 0, subresource_count, 0, footprints.data(), row_counts.data(),
                row_sizes.data(), &upload_size);

            auto upload = dxutl::create_buffer(
                device, upload_size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
            auto* mapped = static_cast<std::byte*>(dxutl::map_upload_buffer(upload.Get()));

            for (size_t item = 0; item < loaded.metadata.arraySize; ++item) {
                for (size_t mip = 0; mip < loaded.metadata.mipLevels; ++mip) {
                    const UINT subresource = static_cast<UINT>(item * loaded.metadata.mipLevels + mip);
                    const DirectX::Image* image = loaded.image.GetImage(mip, item, 0);
                    if (image == nullptr) {
                        upload->Unmap(0, nullptr);
                        util::Logger::g_logger.assert_with_log(
                            false, "Donut material texture has a missing subresource");
                    }

                    std::byte* target = mapped + footprints[subresource].Offset;
                    for (UINT row = 0; row < row_counts[subresource]; ++row) {
                        std::memcpy(
                            target + static_cast<size_t>(row) * footprints[subresource].Footprint.RowPitch,
                            image->pixels + static_cast<size_t>(row) * image->rowPitch,
                            static_cast<size_t>(row_sizes[subresource]));
                    }
                }
            }
            upload->Unmap(0, nullptr);

            for (UINT subresource = 0; subresource < subresource_count; ++subresource) {
                D3D12_TEXTURE_COPY_LOCATION source_location{};
                source_location.pResource = upload.Get();
                source_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                source_location.PlacedFootprint = footprints[subresource];

                D3D12_TEXTURE_COPY_LOCATION target_location{};
                target_location.pResource = texture.Get();
                target_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                target_location.SubresourceIndex = subresource;
                command_list->CopyTextureRegion(
                    &target_location, 0, 0, 0, &source_location, nullptr);
            }

            dxutl::transition_resource(
                command_list, texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

            const uint32_t texture_index = static_cast<uint32_t>(destination.textures.size());
            destination.textures.emplace_back(std::move(texture));
            used_upload_heaps.emplace_back(std::move(upload));
            cache.emplace(cache_key, texture_index);
            return texture_index;
        }

        void clear_descriptor_indices(dnt::MaterialConstants& material) {
            material.baseOrDiffuseTextureIndex = invalid_descriptor_index;
            material.metalRoughOrSpecularTextureIndex = invalid_descriptor_index;
            material.emissiveTextureIndex = invalid_descriptor_index;
            material.normalTextureIndex = invalid_descriptor_index;
            material.occlusionTextureIndex = invalid_descriptor_index;
            material.transmissionTextureIndex = invalid_descriptor_index;
            material.opacityTextureIndex = invalid_descriptor_index;
        }
    }

    std::unique_ptr<DonutSceneDataGPU> DonutSceneResourceBuilder::build(
        const DonutSceneDataCPU& source,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps) {
        util::Logger::g_logger.assert_with_log(
            device != nullptr && command_list != nullptr,
            "Donut scene resource builder requires a device and command list");

        std::string validation_error;
        const bool source_is_valid = source.validate(validation_error);
        util::Logger::g_logger.assert_with_log(
            source_is_valid, validation_error.c_str());

        auto destination = std::make_unique<DonutSceneDataGPU>();
        std::vector<std::byte> vertex_data;
        append_vertex_stream(vertex_data, source.positions, destination->vertex_layout.position_offset);
        append_vertex_stream(vertex_data, source.prev_positions, destination->vertex_layout.prev_position_offset);
        append_vertex_stream(vertex_data, source.packed_normals, destination->vertex_layout.normal_offset);
        append_vertex_stream(vertex_data, source.packed_tangents, destination->vertex_layout.tangent_offset);
        append_vertex_stream(vertex_data, source.texcoords, destination->vertex_layout.texcoord_offset);
        destination->vertex_layout.byte_size = to_uint32(
            vertex_data.size(), "Donut vertex buffer exceeds 32-bit addressing");

        util::Logger::g_logger.assert_with_log(
            source.indices.size() <= (std::numeric_limits<UINT>::max)() / sizeof(uint32_t),
            "Donut index buffer exceeds D3D12 limits");

        destination->geometry_data.reserve(source.geometries.size());
        for (const DonutSceneDataCPU::Geometry& source_geometry : source.geometries) {
            dnt::GeometryData geometry{};
            geometry.numIndices = source_geometry.index_count;
            geometry.numVertices = source_geometry.vertex_count;
            geometry.indexBufferIndex = invalid_descriptor_index;
            geometry.indexOffset = source_geometry.index_offset * sizeof(uint32_t);
            geometry.vertexBufferIndex = invalid_descriptor_index;
            geometry.positionOffset = destination->vertex_layout.position_offset +
                source_geometry.vertex_offset * sizeof(DirectX::XMFLOAT3);
            geometry.prevPositionOffset = destination->vertex_layout.prev_position_offset +
                source_geometry.vertex_offset * sizeof(DirectX::XMFLOAT3);
            geometry.texCoord1Offset = destination->vertex_layout.texcoord_offset +
                source_geometry.vertex_offset * sizeof(DirectX::XMFLOAT2);
            geometry.texCoord2Offset = invalid_index;
            geometry.normalOffset = destination->vertex_layout.normal_offset +
                source_geometry.vertex_offset * sizeof(uint32_t);
            geometry.tangentOffset = destination->vertex_layout.tangent_offset +
                source_geometry.vertex_offset * sizeof(uint32_t);
            geometry.curveRadiusOffset = invalid_index;
            geometry.materialIndex = source_geometry.material_index;
            destination->geometry_data.push_back(geometry);
        }

        std::vector<dnt::InstanceData> instance_data;
        instance_data.reserve(source.instances.size());
        for (const DonutSceneDataCPU::Instance& source_instance : source.instances) {
            const DonutSceneDataCPU::Mesh& mesh = source.meshes[source_instance.mesh_index];
            dnt::InstanceData instance{};
            instance.flags = 0;
            instance.firstGeometryInstanceIndex = source_instance.first_geometry_instance;
            instance.firstGeometryIndex = mesh.first_geometry;
            instance.numGeometries = mesh.geometry_count;
            instance.transform = source_instance.transform;
            instance.prevTransform = source_instance.prev_transform;
            instance_data.push_back(instance);
        }

        destination->draws.reserve(source.draws.size());
        for (const DonutSceneDataCPU::Draw& source_draw : source.draws) {
            const DonutSceneDataCPU::Geometry& geometry =
                source.geometries[source_draw.geometry_index];
            destination->draws.push_back({
                geometry.index_count,
                geometry.index_offset,
                0,
                source_draw.instance_index,
                geometry.vertex_offset,
                geometry.material_index
            });
        }

        create_fallback_texture(
            device, command_list, *destination, used_upload_heaps);
        util::Logger::g_logger.assert_with_log(
            destination->textures.size() == fallback_texture_resource + 1,
            "Donut fallback texture index is invalid");

        std::unordered_map<std::string, uint32_t> texture_cache;
        destination->material_data.reserve(source.materials.size());
        destination->material_texture_resources.reserve(source.materials.size());
        for (const DonutSceneDataCPU::Material& source_material : source.materials) {
            dnt::MaterialConstants material = source_material.constants;
            clear_descriptor_indices(material);
            std::array<uint32_t, material_texture_slot_count> texture_resources{};

            for (uint32_t slot_index = 0; slot_index < material_texture_slot_count; ++slot_index) {
                const MaterialTextureSlot slot = static_cast<MaterialTextureSlot>(slot_index);
                const std::optional<uint32_t> texture = load_texture(
                    source,
                    source_material.textures[slot_index],
                    texture_is_srgb(slot),
                    device,
                    command_list,
                    *destination,
                    texture_cache,
                    used_upload_heaps);
                if (texture) {
                    texture_resources[slot_index] = *texture;
                } else {
                    texture_resources[slot_index] = fallback_texture_resource;
                    material.flags &= ~texture_flag(slot);
                }
            }

            destination->material_data.push_back(material);
            destination->material_texture_resources.push_back(texture_resources);
        }

        const size_t index_byte_size = source.indices.size() * sizeof(uint32_t);
        const size_t geometry_byte_size = destination->geometry_data.size() * sizeof(dnt::GeometryData);
        const size_t instance_byte_size = instance_data.size() * sizeof(dnt::InstanceData);
        const size_t material_byte_size = destination->material_data.size() * sizeof(dnt::MaterialConstants);

        util::Logger::g_logger.assert_with_log(
            index_byte_size <= (std::numeric_limits<UINT>::max)(),
            "Donut index buffer view exceeds D3D12 limits");

        upload_buffer(
            device, command_list, vertex_data.data(), vertex_data.size(),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination->vertex_buffer, used_upload_heaps);
        upload_buffer(
            device, command_list, source.indices.data(), index_byte_size,
            D3D12_RESOURCE_STATE_INDEX_BUFFER | D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination->index_buffer, used_upload_heaps);
        upload_buffer(
            device, command_list, destination->geometry_data.data(), geometry_byte_size,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination->geometry_buffer, used_upload_heaps);
        upload_buffer(
            device, command_list, instance_data.data(), instance_byte_size,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination->instance_buffer, used_upload_heaps);
        upload_buffer(
            device, command_list, destination->material_data.data(), material_byte_size,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination->material_buffer, used_upload_heaps);

        destination->material_constant_buffers.reserve(destination->material_data.size());
        for (const dnt::MaterialConstants& material : destination->material_data) {
            std::array<std::byte, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT> constant_data{};
            std::memcpy(constant_data.data(), &material, sizeof(material));

            Microsoft::WRL::ComPtr<ID3D12Resource> material_constant_buffer;
            upload_buffer(
                device, command_list, constant_data.data(), constant_data.size(),
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
                material_constant_buffer, used_upload_heaps);
            destination->material_constant_buffers.push_back(std::move(material_constant_buffer));
        }

        destination->index_buffer_view.BufferLocation =
            destination->index_buffer->GetGPUVirtualAddress();
        destination->index_buffer_view.SizeInBytes = static_cast<UINT>(index_byte_size);
        destination->index_buffer_view.Format = DXGI_FORMAT_R32_UINT;

        return destination;
    }
}
