#pragma once

#include <cstdint>
#include <filesystem>

#include "scene/data/source/SceneConstants.h"

namespace scene::source {

    enum class ImageFormat : uint8_t {
        Unknown,
        Jpeg,
        Png,
        WebP,
        Ktx2,
        Dds
    };

    // Encoded images stay in their source file. A GLB image uses byte_offset
    // and byte_size to identify its encoded buffer-view payload without
    // duplicating hundreds of megabytes in SceneSourceData.
    struct Image {
        std::filesystem::path path;
        uint64_t byte_offset = 0;
        uint64_t byte_size = 0;
        ImageFormat format = ImageFormat::Unknown;

        bool is_file_range() const noexcept {
            return byte_size != 0;
        }
    };

    enum class TextureFilter : uint16_t {
        Unspecified = 0,
        Nearest = 9728,
        Linear = 9729,
        NearestMipNearest = 9984,
        LinearMipNearest = 9985,
        NearestMipLinear = 9986,
        LinearMipLinear = 9987
    };

    enum class TextureWrap : uint16_t {
        Repeat = 10497,
        ClampToEdge = 33071,
        MirroredRepeat = 33648
    };

    struct Sampler {
        TextureFilter mag_filter = TextureFilter::Unspecified;
        TextureFilter min_filter = TextureFilter::Unspecified;
        TextureWrap wrap_u = TextureWrap::Repeat;
        TextureWrap wrap_v = TextureWrap::Repeat;
    };

    struct Texture {
        uint32_t image_id = SceneConstants::INVALID_INDEX;
        uint32_t sampler_id = SceneConstants::INVALID_INDEX;
    };

    struct TextureRef {
        uint32_t texture_id = SceneConstants::INVALID_INDEX;
        uint32_t uv_set = 0;

        bool valid() const noexcept {
            return texture_id != SceneConstants::INVALID_INDEX;
        }
    };
}
