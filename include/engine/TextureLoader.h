#pragma once

#include <filesystem>
#include <string>

#include <DirectXTex.h>

namespace eng {

    struct TextureLoadResult {
        DirectX::ScratchImage image;
        DirectX::TexMetadata metadata{};
        HRESULT status = E_FAIL;
        std::string error_message;

        [[nodiscard]] bool succeeded() const noexcept {
            return SUCCEEDED(status);
        }
    };

    class TextureLoader {
    public:
        static void init();
        static void close();

        [[nodiscard]] static TextureLoadResult load(
            const std::filesystem::path& path);
    };
}
