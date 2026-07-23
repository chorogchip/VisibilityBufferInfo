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

        using MaterialTextureSlot = DonutSceneDataCPU::EnumMaterialTextureSlot;
        using TextureColorSpace = DonutSceneDataCPU::EnumTextureColorSpace;
        using TextureFallback = DonutSceneDataCPU::EnumTextureFallback;

        size_t align_16(size_t value) {
            return (value + 15u) & ~size_t(15u);
        }

        size_t align_constant_buffer(size_t value) {
            return (value + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1u) &
                ~size_t(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1u);
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
            const size_t stream_size = source.size() * sizeof(T);
            offset = to_uint32(
                stream_offset,
                "Donut vertex stream offset exceeds 32-bit addressing");
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
                device != nullptr && command_list != nullptr && source != nullptr,
                "Donut scene buffer upload requires valid inputs");
            util::Logger::g_logger.assert_with_log(
                byte_size > 0, "Donut scene buffer must not be empty");

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

        DirectX::XMFLOAT3X4 to_shader_float3x4(
            const DirectX::XMFLOAT4X4& matrix) {
            return {
                matrix._11, matrix._21, matrix._31, matrix._41,
                matrix._12, matrix._22, matrix._32, matrix._42,
                matrix._13, matrix._23, matrix._33, matrix._43
            };
        }

        uint32_t create_fallback_texture(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            DonutSceneDataGPU& destination,
            const std::array<uint8_t, 4>& pixel,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps) {

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
                device, texture_desc, D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COPY_DEST);

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
            UINT row_count = 0;
            UINT64 row_size = 0;
            UINT64 upload_size = 0;
            device->GetCopyableFootprints(
                &texture_desc, 0, 1, 0,
                &footprint, &row_count, &row_size, &upload_size);

            (void)row_count;
            (void)row_size;

            auto upload = dxutl::create_buffer(
                device, upload_size, D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_STATE_GENERIC_READ);
            auto* mapped = static_cast<std::byte*>(
                dxutl::map_upload_buffer(upload.Get()));
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

            const uint32_t texture_index =
                static_cast<uint32_t>(destination.textures.size());
            destination.textures.emplace_back(std::move(texture));
            used_upload_heaps.emplace_back(std::move(upload));
            return texture_index;
        }

        uint32_t fallback_texture_index(
            const DonutSceneDataGPU& destination,
            TextureFallback fallback) {
            switch (fallback) {
            case DonutSceneDataCPU::EnumTextureFallback::WHITE:
                return destination.fallback_texture_indices[0];
            case DonutSceneDataCPU::EnumTextureFallback::BLACK:
                return destination.fallback_texture_indices[1];
            case DonutSceneDataCPU::EnumTextureFallback::FLAT_NORMAL:
                return destination.fallback_texture_indices[2];
            }

            return destination.fallback_texture_indices[0];
        }

        bool texture_is_srgb(TextureColorSpace color_space) {
            return color_space == DonutSceneDataCPU::EnumTextureColorSpace::SRGB;
        }

        TextureFallback fallback_for_slot(MaterialTextureSlot slot) {
            switch (slot) {
            case DonutSceneDataCPU::EnumMaterialTextureSlot::BASE_COLOR:
            case DonutSceneDataCPU::EnumMaterialTextureSlot::METAL_ROUGHNESS:
            case DonutSceneDataCPU::EnumMaterialTextureSlot::OCCLUSION:
            case DonutSceneDataCPU::EnumMaterialTextureSlot::TRANSMISSION:
            case DonutSceneDataCPU::EnumMaterialTextureSlot::OPACITY:
                return DonutSceneDataCPU::EnumTextureFallback::WHITE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::NORMAL:
                return DonutSceneDataCPU::EnumTextureFallback::FLAT_NORMAL;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::EMISSIVE:
                return DonutSceneDataCPU::EnumTextureFallback::BLACK;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::COUNT:
                break;
            }
            return DonutSceneDataCPU::EnumTextureFallback::WHITE;
        }

        std::optional<uint32_t> load_texture(
            const DonutSceneDataCPU& source,
            const DonutSceneDataCPU::Texture& source_texture,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            DonutSceneDataGPU& destination,
            std::unordered_map<std::string, uint32_t>& cache,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps,
            bool load_textures) {

            if (!load_textures || source_texture.embedded || source_texture.path.empty()) {
                return std::nullopt;
            }

            std::filesystem::path resolved_path = source_texture.path;
            if (resolved_path.is_relative()) {
                resolved_path = source.source_path.parent_path() / resolved_path;
            }
            resolved_path = resolved_path.lexically_normal();

            const bool srgb = texture_is_srgb(source_texture.color_space);
            const std::string cache_key = resolved_path.generic_string() +
                (srgb ? "|srgb" : "|linear");
            const auto cached = cache.find(cache_key);
            if (cached != cache.end()) return cached->second;

            eng::TextureLoadResult loaded = eng::TextureLoader::load(resolved_path);
            if (!loaded.succeeded()) {
                util::Logger::g_logger << loaded.error_message << '\n';
                return std::nullopt;
            }

            if (loaded.metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D ||
                DirectX::IsPlanar(loaded.metadata.format) ||
                loaded.metadata.arraySize != 1 ||
                loaded.metadata.arraySize > (std::numeric_limits<UINT16>::max)() ||
                loaded.metadata.mipLevels > (std::numeric_limits<UINT16>::max)() ||
                loaded.metadata.height > (std::numeric_limits<UINT>::max)()) {
                util::Logger::g_logger
                    << "Unsupported Donut material texture: "
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
                device, texture_desc, D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COPY_DEST);

            const size_t subresource_count_size =
                loaded.metadata.arraySize * loaded.metadata.mipLevels;
            const UINT subresource_count = to_uint(
                subresource_count_size,
                "Donut texture has too many subresources");
            std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(subresource_count);
            std::vector<UINT> row_counts(subresource_count);
            std::vector<UINT64> row_sizes(subresource_count);
            UINT64 upload_size = 0;
            device->GetCopyableFootprints(
                &texture_desc, 0, subresource_count, 0,
                footprints.data(), row_counts.data(), row_sizes.data(), &upload_size);

            auto upload = dxutl::create_buffer(
                device, upload_size, D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_STATE_GENERIC_READ);
            auto* mapped = static_cast<std::byte*>(
                dxutl::map_upload_buffer(upload.Get()));

            for (size_t item = 0; item < loaded.metadata.arraySize; ++item) {
                for (size_t mip = 0; mip < loaded.metadata.mipLevels; ++mip) {
                    const UINT subresource =
                        static_cast<UINT>(item * loaded.metadata.mipLevels + mip);
                    const DirectX::Image* image = loaded.image.GetImage(mip, item, 0);
                    util::Logger::g_logger.assert_with_log(
                        image != nullptr,
                        "Donut material texture has a missing subresource");

                    std::byte* target = mapped + footprints[subresource].Offset;
                    for (UINT row = 0; row < row_counts[subresource]; ++row) {
                        std::memcpy(
                            target +
                            static_cast<size_t>(row) *
                            footprints[subresource].Footprint.RowPitch,
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

            const uint32_t texture_index =
                static_cast<uint32_t>(destination.textures.size());
            destination.textures.emplace_back(std::move(texture));
            used_upload_heaps.emplace_back(std::move(upload));
            cache.emplace(cache_key, texture_index);
            return texture_index;
        }

        uint32_t material_texture_flag(MaterialTextureSlot slot) {
            switch (slot) {
            case DonutSceneDataCPU::EnumMaterialTextureSlot::BASE_COLOR:
                return DonutSceneDataGPU::MATERIAL_FLAG_BASE_COLOR_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::METAL_ROUGHNESS:
                return DonutSceneDataGPU::MATERIAL_FLAG_METAL_ROUGHNESS_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::NORMAL:
                return DonutSceneDataGPU::MATERIAL_FLAG_NORMAL_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::EMISSIVE:
                return DonutSceneDataGPU::MATERIAL_FLAG_EMISSIVE_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::OCCLUSION:
                return DonutSceneDataGPU::MATERIAL_FLAG_OCCLUSION_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::TRANSMISSION:
                return DonutSceneDataGPU::MATERIAL_FLAG_TRANSMISSION_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::OPACITY:
                return DonutSceneDataGPU::MATERIAL_FLAG_OPACITY_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::COUNT:
                break;
            }
            return 0;
        }

        int32_t shader_material_texture_flag(MaterialTextureSlot slot) {
            switch (slot) {
            case DonutSceneDataCPU::EnumMaterialTextureSlot::BASE_COLOR:
                return DonutSceneDataGPU::SHADER_MATERIAL_FLAG_USE_BASE_COLOR_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::METAL_ROUGHNESS:
                return DonutSceneDataGPU::SHADER_MATERIAL_FLAG_USE_METAL_ROUGHNESS_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::NORMAL:
                return DonutSceneDataGPU::SHADER_MATERIAL_FLAG_USE_NORMAL_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::EMISSIVE:
                return DonutSceneDataGPU::SHADER_MATERIAL_FLAG_USE_EMISSIVE_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::OCCLUSION:
                return DonutSceneDataGPU::SHADER_MATERIAL_FLAG_USE_OCCLUSION_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::TRANSMISSION:
                return DonutSceneDataGPU::SHADER_MATERIAL_FLAG_USE_TRANSMISSION_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::OPACITY:
                return DonutSceneDataGPU::SHADER_MATERIAL_FLAG_USE_OPACITY_TEXTURE;
            case DonutSceneDataCPU::EnumMaterialTextureSlot::COUNT:
                break;
            }
            return 0;
        }

        int32_t shader_material_domain(
            DonutSceneDataCPU::EnumMaterialDomain domain) {

            switch (domain) {
            case DonutSceneDataCPU::EnumMaterialDomain::Opaque:
                return DonutSceneDataGPU::SHADER_MATERIAL_DOMAIN_OPAQUE;
            case DonutSceneDataCPU::EnumMaterialDomain::AlphaTested:
                return DonutSceneDataGPU::SHADER_MATERIAL_DOMAIN_ALPHA_TESTED;
            case DonutSceneDataCPU::EnumMaterialDomain::AlphaBlended:
                return DonutSceneDataGPU::SHADER_MATERIAL_DOMAIN_ALPHA_BLENDED;
            }
            return DonutSceneDataGPU::SHADER_MATERIAL_DOMAIN_OPAQUE;
        }

        DonutSceneDataGPU::ShaderMaterialConstants make_shader_material_constants(
            const DonutSceneDataGPU::MaterialData& source,
            uint32_t material_id) {

            DonutSceneDataGPU::ShaderMaterialConstants constants{};
            constants.base_or_diffuse_color = {
                source.base_color.x,
                source.base_color.y,
                source.base_color.z
            };
            constants.specular_color = { 0.04f, 0.04f, 0.04f };
            constants.material_id = static_cast<int32_t>(material_id);
            constants.emissive_color = source.emissive_color;
            constants.domain = source.domain;
            constants.opacity = source.base_color.w;
            constants.roughness = source.roughness;
            constants.metalness = source.metalness;
            constants.normal_texture_scale = source.normal_scale;
            constants.occlusion_strength = source.occlusion_strength;
            constants.alpha_cutoff = source.alpha_cutoff;
            constants.transmission_factor = 0.0f;
            constants.normal_texture_transform_scale = { 1.0f, 1.0f };

            if ((source.flags & DonutSceneDataGPU::MATERIAL_FLAG_DOUBLE_SIDED) != 0)
                constants.flags |= DonutSceneDataGPU::SHADER_MATERIAL_FLAG_DOUBLE_SIDED;

            for (uint32_t slot_index = 0;
                slot_index < DonutSceneDataCPU::MATERIAL_TEXTURE_SLOT_COUNT;
                ++slot_index) {
                const MaterialTextureSlot slot =
                    static_cast<MaterialTextureSlot>(slot_index);
                if ((source.flags & material_texture_flag(slot)) != 0)
                    constants.flags |= shader_material_texture_flag(slot);
            }

            constants.base_or_diffuse_texture_index =
                static_cast<int32_t>(source.texture_indices[
                    static_cast<size_t>(DonutSceneDataCPU::EnumMaterialTextureSlot::BASE_COLOR)]);
            constants.metal_rough_or_specular_texture_index =
                static_cast<int32_t>(source.texture_indices[
                    static_cast<size_t>(DonutSceneDataCPU::EnumMaterialTextureSlot::METAL_ROUGHNESS)]);
            constants.emissive_texture_index =
                static_cast<int32_t>(source.texture_indices[
                    static_cast<size_t>(DonutSceneDataCPU::EnumMaterialTextureSlot::EMISSIVE)]);
            constants.normal_texture_index =
                static_cast<int32_t>(source.texture_indices[
                    static_cast<size_t>(DonutSceneDataCPU::EnumMaterialTextureSlot::NORMAL)]);
            constants.occlusion_texture_index =
                static_cast<int32_t>(source.texture_indices[
                    static_cast<size_t>(DonutSceneDataCPU::EnumMaterialTextureSlot::OCCLUSION)]);
            constants.transmission_texture_index =
                static_cast<int32_t>(source.texture_indices[
                    static_cast<size_t>(DonutSceneDataCPU::EnumMaterialTextureSlot::TRANSMISSION)]);
            constants.opacity_texture_index =
                static_cast<int32_t>(source.texture_indices[
                    static_cast<size_t>(DonutSceneDataCPU::EnumMaterialTextureSlot::OPACITY)]);
            return constants;
        }
    }

    void DonutSceneResourceBuilder::rebuild_draw_stream(
        const DonutSceneDataCPU& source,
        const std::vector<DonutSceneDataCPU::VisibleDraw>& visible_draws,
        DonutSceneDataGPU& destination) {

        destination.draws.clear();
        destination.render_instance_data.clear();
        destination.draws.reserve(visible_draws.size());
        destination.render_instance_data.reserve(visible_draws.size());

        for (const DonutSceneDataCPU::VisibleDraw& visible_draw : visible_draws) {
            const DonutSceneDataCPU::GeometryInstance& geometry_instance =
                source.geometry_instances[visible_draw.geometry_instance_id];
            const DonutSceneDataCPU::Submesh& submesh =
                source.submeshes[geometry_instance.submesh_id];

            if (destination.draws.empty() ||
                destination.draws.back().submesh_id != geometry_instance.submesh_id ||
                destination.draws.back().material_id != submesh.material_id) {
                destination.draws.push_back({
                    static_cast<uint32_t>(destination.render_instance_data.size()),
                    0,
                    geometry_instance.submesh_id,
                    submesh.index_count,
                    submesh.index_offset,
                    submesh.vertex_offset,
                    submesh.material_id
                    });
            }

            destination.render_instance_data.push_back(
                destination.instance_data[geometry_instance.instance_id]);
            ++destination.draws.back().instance_count;
        }
    }

    std::unique_ptr<DonutSceneDataGPU> DonutSceneResourceBuilder::build(
        const DonutSceneDataCPU& source,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps,
        bool load_textures) {

        util::Logger::g_logger.assert_with_log(
            device != nullptr && command_list != nullptr,
            "Donut scene resource builder requires a device and command list");

        std::string validation_error;
        const bool source_is_valid = source.validate(validation_error);
        util::Logger::g_logger.assert_with_log(
            source_is_valid, validation_error.c_str());

        auto destination = std::make_unique<DonutSceneDataGPU>();

        std::vector<std::byte> vertex_data;
        append_vertex_stream(
            vertex_data, source.positions,
            destination->vertex_layout.position_offset);
        append_vertex_stream(
            vertex_data, source.prev_positions,
            destination->vertex_layout.prev_position_offset);
        append_vertex_stream(
            vertex_data, source.texcoords,
            destination->vertex_layout.texcoord_offset);
        append_vertex_stream(
            vertex_data, source.packed_normals,
            destination->vertex_layout.normal_offset);
        append_vertex_stream(
            vertex_data, source.packed_tangents,
            destination->vertex_layout.tangent_offset);
        destination->vertex_layout.byte_size = to_uint32(
            vertex_data.size(),
            "Donut vertex buffer exceeds 32-bit addressing");

        destination->instance_data.reserve(source.instances.size());
        for (const DonutSceneDataCPU::Instance& source_instance : source.instances) {
            const DonutSceneDataCPU::Mesh& source_mesh =
                source.meshes[source_instance.mesh_id];
            DonutSceneDataGPU::InstanceData instance{};
            instance.first_geometry_instance =
                source_instance.first_geometry_instance;
            instance.first_geometry = source_mesh.first_submesh;
            instance.geometry_instance_count =
                source_instance.geometry_instance_count;
            instance.transform = to_shader_float3x4(source_instance.world_transform);
            instance.prev_transform =
                to_shader_float3x4(source_instance.prev_world_transform);
            destination->instance_data.push_back(instance);
        }

        destination->submesh_data.reserve(source.submeshes.size());
        for (const DonutSceneDataCPU::Submesh& source_submesh : source.submeshes) {
            DonutSceneDataGPU::SubmeshData submesh{};
            submesh.vertex_offset = source_submesh.vertex_offset;
            submesh.vertex_count = source_submesh.vertex_count;
            submesh.index_offset = source_submesh.index_offset;
            submesh.index_count = source_submesh.index_count;
            submesh.material_id = source_submesh.material_id;
            destination->submesh_data.push_back(submesh);
        }

        destination->geometry_instance_data.reserve(source.geometry_instances.size());
        for (const DonutSceneDataCPU::GeometryInstance& source_geometry_instance :
            source.geometry_instances) {
            destination->geometry_instance_data.push_back({
                source_geometry_instance.instance_id,
                source_geometry_instance.submesh_id,
                0,
                0
            });
        }

        destination->fallback_texture_indices[0] = create_fallback_texture(
            device, command_list, *destination,
            { 255, 255, 255, 255 }, used_upload_heaps);
        destination->fallback_texture_indices[1] = create_fallback_texture(
            device, command_list, *destination,
            { 0, 0, 0, 255 }, used_upload_heaps);
        destination->fallback_texture_indices[2] = create_fallback_texture(
            device, command_list, *destination,
            { 128, 128, 255, 255 }, used_upload_heaps);

        std::unordered_map<std::string, uint32_t> texture_cache;
        std::vector<std::optional<uint32_t>> source_texture_to_gpu(source.textures.size());
        for (uint32_t texture_id = 0; texture_id < source.textures.size(); ++texture_id) {
            source_texture_to_gpu[texture_id] = load_texture(
                source,
                source.textures[texture_id],
                device,
                command_list,
                *destination,
                texture_cache,
                used_upload_heaps,
                load_textures);
        }

        destination->material_data.reserve(source.materials.size());
        for (const DonutSceneDataCPU::Material& source_material : source.materials) {
            DonutSceneDataGPU::MaterialData material{};
            material.base_color = source_material.base_color;
            material.emissive_color = source_material.emissive_color;
            material.roughness = source_material.roughness;
            material.metalness = source_material.metalness;
            material.normal_scale = source_material.normal_scale;
            material.occlusion_strength = source_material.occlusion_strength;
            material.alpha_cutoff = source_material.alpha_cutoff;
            material.domain = shader_material_domain(source_material.domain);
            if (source_material.double_sided) {
                material.flags |= DonutSceneDataGPU::MATERIAL_FLAG_DOUBLE_SIDED;
            }

            for (uint32_t slot_index = 0;
                slot_index < DonutSceneDataCPU::MATERIAL_TEXTURE_SLOT_COUNT;
                ++slot_index) {
                const MaterialTextureSlot slot =
                    static_cast<MaterialTextureSlot>(slot_index);
                const uint32_t source_texture_id =
                    source_material.texture_ids[slot_index];

                if (source_texture_id != DonutSceneDataCPU::INVALID_INDEX &&
                    source_texture_id < source_texture_to_gpu.size() &&
                    source_texture_to_gpu[source_texture_id]) {
                    material.texture_indices[slot_index] =
                        *source_texture_to_gpu[source_texture_id];
                    material.flags |= material_texture_flag(slot);
                } else if (source_texture_id != DonutSceneDataCPU::INVALID_INDEX &&
                    source_texture_id < source.textures.size()) {
                    material.texture_indices[slot_index] = fallback_texture_index(
                        *destination,
                        source.textures[source_texture_id].fallback);
                } else {
                    material.texture_indices[slot_index] = fallback_texture_index(
                        *destination,
                        fallback_for_slot(slot));
                }
            }

            destination->material_data.push_back(material);
        }

        std::vector<DonutSceneDataCPU::VisibleDraw> visible_draws;
        source.build_all_visible_draws(visible_draws);
        source.sort_visible_draws(visible_draws);
        rebuild_draw_stream(source, visible_draws, *destination);
        destination->render_instance_capacity =
            static_cast<uint32_t>(destination->render_instance_data.size());

        std::vector<uint32_t> gpu_indices = source.indices;
        for (const DonutSceneDataCPU::Submesh& submesh : source.submeshes) {
            const uint32_t index_end = submesh.index_offset + submesh.index_count;
            for (uint32_t index = submesh.index_offset; index < index_end; ++index) {
                gpu_indices[index] += submesh.vertex_offset;
            }
        }

        const size_t index_byte_size = gpu_indices.size() * sizeof(uint32_t);
        const size_t instance_byte_size =
            destination->instance_data.size() *
            sizeof(DonutSceneDataGPU::InstanceData);
        const size_t render_instance_byte_size =
            destination->render_instance_data.size() *
            sizeof(DonutSceneDataGPU::InstanceData);
        const size_t submesh_byte_size =
            destination->submesh_data.size() *
            sizeof(DonutSceneDataGPU::SubmeshData);
        const size_t geometry_instance_byte_size =
            destination->geometry_instance_data.size() *
            sizeof(DonutSceneDataGPU::GeometryInstanceData);
        const size_t material_byte_size =
            destination->material_data.size() *
            sizeof(DonutSceneDataGPU::MaterialData);

        destination->material_constant_stride = to_uint32(
            align_constant_buffer(sizeof(DonutSceneDataGPU::ShaderMaterialConstants)),
            "Donut material constant stride exceeds 32-bit addressing");
        const size_t material_constant_byte_size =
            destination->material_constant_stride *
            destination->material_data.size();
        std::vector<std::byte> material_constant_data(material_constant_byte_size);
        for (size_t material_index = 0;
            material_index < destination->material_data.size();
            ++material_index) {
            const DonutSceneDataGPU::ShaderMaterialConstants material_constants =
                make_shader_material_constants(
                    destination->material_data[material_index],
                    static_cast<uint32_t>(material_index));
            std::memcpy(
                material_constant_data.data() +
                material_index * destination->material_constant_stride,
                &material_constants,
                sizeof(DonutSceneDataGPU::ShaderMaterialConstants));
        }

        upload_buffer(
            device, command_list, vertex_data.data(), vertex_data.size(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination->vertex_buffer, used_upload_heaps);
        upload_buffer(
            device, command_list, gpu_indices.data(), index_byte_size,
            D3D12_RESOURCE_STATE_INDEX_BUFFER |
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination->index_buffer, used_upload_heaps);
        upload_buffer(
            device, command_list,
            destination->instance_data.data(), instance_byte_size,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination->instance_buffer, used_upload_heaps);
        upload_buffer(
            device, command_list,
            destination->render_instance_data.data(), render_instance_byte_size,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination->render_instance_buffer, used_upload_heaps);
        upload_buffer(
            device, command_list,
            destination->submesh_data.data(), submesh_byte_size,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination->submesh_buffer, used_upload_heaps);
        upload_buffer(
            device, command_list,
            destination->geometry_instance_data.data(), geometry_instance_byte_size,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination->geometry_instance_buffer, used_upload_heaps);
        upload_buffer(
            device, command_list,
            destination->material_data.data(), material_byte_size,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            destination->material_buffer, used_upload_heaps);
        upload_buffer(
            device, command_list,
            material_constant_data.data(), material_constant_data.size(),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            destination->material_constant_buffer, used_upload_heaps);

        destination->index_buffer_view.BufferLocation =
            destination->index_buffer->GetGPUVirtualAddress();
        destination->index_buffer_view.SizeInBytes =
            to_uint(index_byte_size, "Donut index buffer exceeds D3D12 index view size");
        destination->index_buffer_view.Format = DXGI_FORMAT_R32_UINT;

        return destination;
    }
}
