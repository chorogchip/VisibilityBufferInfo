#include "JungleSceneSourceBuilderInternal.h"

#include <optional>
#include <string>
#include <variant>

#include "util/Logger.h"

namespace scene::source::jungle {

    namespace {

        template <class... T>
        struct Overloaded : T... {
            using T::operator()...;
        };
        template <class... T>
        Overloaded(T...) -> Overloaded<T...>;

        ImageFormat convert_image_format(fastgltf::MimeType value) {
            switch (value) {
            case fastgltf::MimeType::JPEG:
                return ImageFormat::Jpeg;
            case fastgltf::MimeType::PNG:
                return ImageFormat::Png;
            case fastgltf::MimeType::WEBP:
                return ImageFormat::WebP;
            case fastgltf::MimeType::KTX2:
                return ImageFormat::Ktx2;
            case fastgltf::MimeType::DDS:
                return ImageFormat::Dds;
            default:
                return ImageFormat::Unknown;
            }
        }

        std::optional<size_t> texture_image_index(
            const fastgltf::Texture& texture) {
            if (texture.webpImageIndex) return *texture.webpImageIndex;
            if (texture.basisuImageIndex) return *texture.basisuImageIndex;
            if (texture.ddsImageIndex) return *texture.ddsImageIndex;
            if (texture.imageIndex) return *texture.imageIndex;
            return std::nullopt;
        }

        void decode_buffer_view_image(
            const Context& context,
            const fastgltf::Asset& asset,
            const fastgltf::sources::BufferView& source,
            Image& image) {

            util::Logger::g_logger.assert_with_log(
                source.bufferViewIndex < asset.bufferViews.size(),
                "glTF image references an invalid buffer view.");
            const fastgltf::BufferView& view =
                asset.bufferViews[source.bufferViewIndex];
            util::Logger::g_logger.assert_with_log(
                view.bufferIndex < asset.buffers.size(),
                "glTF image buffer view references an invalid buffer.");

            image.format = convert_image_format(source.mimeType);
            image.byte_size = view.byteLength;

            if (context.file_layout.binary_offset != 0 &&
                view.bufferIndex == 0) {
                image.path = context.source_path;
                image.byte_offset =
                    context.file_layout.binary_offset + view.byteOffset;
                return;
            }

            const fastgltf::Buffer& buffer =
                asset.buffers[view.bufferIndex];
            const bool decoded = std::visit(Overloaded{
                [&](const fastgltf::sources::URI& uri) {
                    if (!uri.uri.isLocalPath()) return false;
                    image.path =
                        (context.source_path.parent_path() /
                            uri.uri.fspath()).lexically_normal();
                    image.byte_offset =
                        uri.fileByteOffset + view.byteOffset;
                    return true;
                },
                [](const auto&) {
                    return false;
                }
                }, buffer.data);
            util::Logger::g_logger.assert_with_log(
                decoded,
                "Jungle image requires a file-backed buffer.");
        }

        void decode_image(
            const Context& context,
            const fastgltf::Asset& asset,
            const fastgltf::Image& source,
            Image& image) {

            std::visit(Overloaded{
                [&](const fastgltf::sources::BufferView& buffer_view) {
                    decode_buffer_view_image(
                        context,
                        asset,
                        buffer_view,
                        image);
                },
                [&](const fastgltf::sources::URI& uri) {
                    util::Logger::g_logger.assert_with_log(
                        uri.uri.isLocalPath() &&
                        !uri.uri.isDataUri(),
                        "Jungle source builder requires file-backed images.");
                    image.path =
                        (context.source_path.parent_path() /
                            uri.uri.fspath()).lexically_normal();
                    image.byte_offset = uri.fileByteOffset;
                    image.format = convert_image_format(uri.mimeType);
                },
                [&](const auto&) {
                    util::Logger::g_logger.assert_with_log(
                        false,
                        "Jungle source builder requires file-backed images.");
                }
                }, source.data);
        }

        TextureRef convert_texture_ref(
            const fastgltf::TextureInfo& source) {
            TextureRef result{};
            result.texture_id = to_uint32(
                source.textureIndex,
                "glTF texture index exceeds 32-bit indexing.");
            result.uv_set = to_uint32(
                source.texCoordIndex,
                "glTF UV set exceeds 32-bit indexing.");
            if (source.transform && source.transform->texCoordIndex) {
                result.uv_set = to_uint32(
                    *source.transform->texCoordIndex,
                    "glTF transformed UV set exceeds 32-bit indexing.");
            }
            return result;
        }

        AlphaMode convert_alpha_mode(fastgltf::AlphaMode source) {
            switch (source) {
            case fastgltf::AlphaMode::Mask:
                return AlphaMode::Mask;
            case fastgltf::AlphaMode::Blend:
                return AlphaMode::Blend;
            default:
                return AlphaMode::Opaque;
            }
        }

        Material convert_material(const fastgltf::Material& source) {
            Material result{};
            result.name.assign(source.name.data(), source.name.size());
            const fastgltf::math::nvec4& base =
                source.pbrData.baseColorFactor;
            result.base_color = {
                base[0], base[1], base[2], base[3]
            };
            result.emissive_color = {
                source.emissiveFactor[0],
                source.emissiveFactor[1],
                source.emissiveFactor[2]
            };
            result.emissive_intensity = source.emissiveStrength;
            result.metalness = source.pbrData.metallicFactor;
            result.roughness = source.pbrData.roughnessFactor;
            result.alpha_cutoff = source.alphaCutoff;
            result.alpha_mode = convert_alpha_mode(source.alphaMode);
            result.double_sided = source.doubleSided;
            result.ior = source.ior;

            if (source.pbrData.baseColorTexture) {
                result.base_color_texture = convert_texture_ref(
                    *source.pbrData.baseColorTexture);
            }
            if (source.pbrData.metallicRoughnessTexture) {
                result.metal_roughness_texture =
                    convert_texture_ref(
                        *source.pbrData.metallicRoughnessTexture);
            }
            if (source.normalTexture) {
                result.normal_texture =
                    convert_texture_ref(*source.normalTexture);
                result.normal_scale = source.normalTexture->scale;
            }
            if (source.emissiveTexture) {
                result.emissive_texture =
                    convert_texture_ref(*source.emissiveTexture);
            }
            if (source.occlusionTexture) {
                result.occlusion_texture =
                    convert_texture_ref(*source.occlusionTexture);
                result.occlusion_strength =
                    source.occlusionTexture->strength;
            }
            if (source.transmission) {
                result.transmission =
                    source.transmission->transmissionFactor;
                if (source.transmission->transmissionTexture) {
                    result.transmission_texture =
                        convert_texture_ref(
                            *source.transmission->
                                transmissionTexture);
                }
            }
            if (source.specular) {
                result.specular = source.specular->specularFactor;
                result.specular_color = {
                    source.specular->specularColorFactor[0],
                    source.specular->specularColorFactor[1],
                    source.specular->specularColorFactor[2]
                };
            }
            return result;
        }
    }

    void append_materials(
        const Context& context,
        const fastgltf::Asset& asset,
        SceneSourceData& scene) {

        scene.images.reserve(asset.images.size());
        for (const fastgltf::Image& source : asset.images) {
            Image image{};
            decode_image(
                context,
                asset,
                source,
                image);
            scene.images.emplace_back(std::move(image));
        }

        scene.samplers.reserve(asset.samplers.size());
        for (const fastgltf::Sampler& source : asset.samplers) {
            Sampler sampler{};
            if (source.magFilter) {
                sampler.mag_filter =
                    static_cast<TextureFilter>(*source.magFilter);
            }
            if (source.minFilter) {
                sampler.min_filter =
                    static_cast<TextureFilter>(*source.minFilter);
            }
            sampler.wrap_u =
                static_cast<TextureWrap>(source.wrapS);
            sampler.wrap_v =
                static_cast<TextureWrap>(source.wrapT);
            scene.samplers.emplace_back(sampler);
        }

        scene.textures.reserve(asset.textures.size());
        for (const fastgltf::Texture& source : asset.textures) {
            const std::optional<size_t> image_index =
                texture_image_index(source);
            util::Logger::g_logger.assert_with_log(
                image_index &&
                *image_index < scene.images.size(),
                "glTF texture references an invalid image.");

            Texture texture{};
            texture.image_id = to_uint32(
                *image_index,
                "glTF image index exceeds 32-bit indexing.");
            if (source.samplerIndex) {
                texture.sampler_id = to_uint32(
                    *source.samplerIndex,
                    "glTF sampler index exceeds 32-bit indexing.");
            }
            scene.textures.emplace_back(texture);
        }

        scene.materials.reserve(asset.materials.size() + 1);
        for (const fastgltf::Material& source : asset.materials) {
            scene.materials.emplace_back(convert_material(source));
        }
        if (scene.materials.empty()) {
            scene.materials.emplace_back();
        }
    }
}
