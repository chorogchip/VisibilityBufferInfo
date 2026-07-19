#include "engine/TextureLoader.h"

#include <format>

#include "util/Utils.h"
#include "util/StringUtils.h"

namespace eng {
    void TextureLoader::init() {
        Utils::throw_if_failed(
            CoInitializeEx(nullptr, COINIT_MULTITHREADED),
            "initialize COM for texture loading");
    }

    void TextureLoader::close() {
        CoUninitialize();
    }

    TextureLoadResult TextureLoader::load(const std::filesystem::path& path) {
        TextureLoadResult result;

        if (path.empty()) {
            result.status = E_INVALIDARG;
            result.error_message = "Texture path must not be empty.";
            return result;
        }

        if (!std::filesystem::is_regular_file(path)) {
            result.status = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
            result.error_message = "Texture file does not exist: " + path.string();
            return result;
        }

        const std::wstring extension = path.extension().wstring();
        if (util::StringUtils::equals_ignore_case(extension, L".dds")) {
            result.status = DirectX::LoadFromDDSFile(
                path.c_str(), DirectX::DDS_FLAGS_NONE,
                &result.metadata, result.image);
        }
        else if (util::StringUtils::equals_ignore_case(extension, L".tga")) {
            result.status = DirectX::LoadFromTGAFile(
                path.c_str(), DirectX::TGA_FLAGS_NONE,
                &result.metadata, result.image);
        }
        else if (util::StringUtils::equals_ignore_case(extension, L".hdr")) {
            result.status = DirectX::LoadFromHDRFile(
                path.c_str(), &result.metadata, result.image);
        }
        else {
            result.status = DirectX::LoadFromWICFile(
                path.c_str(), DirectX::WIC_FLAGS_NONE,
                &result.metadata, result.image);
        }

        if (FAILED(result.status)) {
            result.error_message = std::format(
                "Failed to load texture: {}, HRESULT=0x{:08X}",
                path.string(), static_cast<uint32_t>(result.status));
        }

        return result;
    }

}
