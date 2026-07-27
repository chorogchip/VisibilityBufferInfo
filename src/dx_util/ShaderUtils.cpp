#include "dx_util/ShaderUtils.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "util/Utils.h"

namespace {

    struct ProgramArgumentDefines {
        std::string gbuffer_count_define;
        std::string texture_count_define;
        std::string texture_sampling_count_define;
        std::string texture_size_define;
        std::string alu_calc_count_define;
        std::array<D3D_SHADER_MACRO, 6> defines{};

        explicit ProgramArgumentDefines(const util::ProgramArgument& args)
            : gbuffer_count_define(std::to_string(args.gbuffer_cnt)),
            texture_count_define(std::to_string(args.texture_count)),
            texture_sampling_count_define(
                std::to_string(args.texture_sampling_count)),
            texture_size_define(std::to_string(args.texture_size)),
            alu_calc_count_define(std::to_string(args.alu_calc_count)),
            defines{ {
                { "GBUFFER_COUNT", gbuffer_count_define.c_str() },
                { "TEXTURE_COUNT", texture_count_define.c_str() },
                { "TEXTURE_SAMPLING_COUNT",
                    texture_sampling_count_define.c_str() },
                { "TEXTURE_SIZE", texture_size_define.c_str() },
                { "ALU_CALC_COUNT", alu_calc_count_define.c_str() },
                { nullptr, nullptr }
            } } {
        }
    };

    std::wstring widen(std::string_view text) {
        if (text.empty()) {
            return {};
        }

        const int size = MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0);
        std::wstring result(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            size);
        return result;
    }

    std::string normalize_target_profile(const char* target) {
        std::string profile = target != nullptr ? target : "";
        if (profile.ends_with("_5_0") || profile.ends_with("_5_1")) {
            profile.replace(profile.size() - 3, 3, "6_6");
        }
        return profile;
    }

    void append_argument(
        std::vector<std::wstring>& storage,
        std::vector<LPCWSTR>& arguments,
        std::wstring value) {

        storage.emplace_back(std::move(value));
        arguments.push_back(storage.back().c_str());
    }

    void append_define_arguments(
        const D3D_SHADER_MACRO* defines,
        std::vector<std::wstring>& storage,
        std::vector<LPCWSTR>& arguments) {

        if (defines == nullptr) {
            return;
        }

        for (const D3D_SHADER_MACRO* define = defines;
            define->Name != nullptr;
            ++define) {

            std::string value = define->Name;
            if (define->Definition != nullptr) {
                value += "=";
                value += define->Definition;
            }

            append_argument(storage, arguments, L"-D");
            append_argument(storage, arguments, widen(value));
        }
    }
}

namespace dxutl {

    Microsoft::WRL::ComPtr<IDxcBlob> compile_shader(
        const std::wstring& path,
        const char* target,
        const char* entry_point,
        const D3D_SHADER_MACRO* defines) {

        Microsoft::WRL::ComPtr<IDxcUtils> utils;
        util::Utils::throw_if_failed(
            DxcCreateInstance(
                CLSID_DxcUtils,
                IID_PPV_ARGS(utils.ReleaseAndGetAddressOf())),
            "create DXC utils");

        Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
        util::Utils::throw_if_failed(
            DxcCreateInstance(
                CLSID_DxcCompiler,
                IID_PPV_ARGS(compiler.ReleaseAndGetAddressOf())),
            "create DXC compiler");

        Microsoft::WRL::ComPtr<IDxcIncludeHandler> include_handler;
        util::Utils::throw_if_failed(
            utils->CreateDefaultIncludeHandler(
                include_handler.ReleaseAndGetAddressOf()),
            "create DXC include handler");

        Microsoft::WRL::ComPtr<IDxcBlobEncoding> source;
        util::Utils::throw_if_failed(
            utils->LoadFile(path.c_str(), nullptr, source.ReleaseAndGetAddressOf()),
            "load shader source");

        const std::string normalized_target = normalize_target_profile(target);

        std::vector<std::wstring> argument_storage;
        argument_storage.reserve(16);
        std::vector<LPCWSTR> arguments;
        arguments.reserve(16);

        append_argument(argument_storage, arguments, path);
        append_argument(argument_storage, arguments, L"-E");
        append_argument(argument_storage, arguments, widen(entry_point));
        append_argument(argument_storage, arguments, L"-T");
        append_argument(argument_storage, arguments, widen(normalized_target));

#if defined(_DEBUG)
        append_argument(argument_storage, arguments, L"-Zi");
        append_argument(argument_storage, arguments, L"-Od");
        append_argument(argument_storage, arguments, L"-Qembed_debug");
#else
        append_argument(argument_storage, arguments, L"-O3");
#endif

        append_define_arguments(defines, argument_storage, arguments);

        DxcBuffer source_buffer{};
        source_buffer.Ptr = source->GetBufferPointer();
        source_buffer.Size = source->GetBufferSize();
        source_buffer.Encoding = DXC_CP_ACP;

        Microsoft::WRL::ComPtr<IDxcResult> result;
        util::Utils::throw_if_failed(
            compiler->Compile(
                &source_buffer,
                arguments.data(),
                static_cast<UINT32>(arguments.size()),
                include_handler.Get(),
                IID_PPV_ARGS(result.ReleaseAndGetAddressOf())),
            "run DXC compiler");

        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
        util::Utils::throw_if_failed(
            result->GetOutput(
                DXC_OUT_ERRORS,
                IID_PPV_ARGS(errors.ReleaseAndGetAddressOf()),
                nullptr),
            "get DXC errors");
        if (errors && errors->GetStringLength() > 0) {
            OutputDebugStringA(errors->GetStringPointer());
        }

        HRESULT status = S_OK;
        util::Utils::throw_if_failed(
            result->GetStatus(&status),
            "get DXC compile status");
        util::Utils::throw_if_failed(status, "compile shader");

        Microsoft::WRL::ComPtr<IDxcBlob> shader;
        util::Utils::throw_if_failed(
            result->GetOutput(
                DXC_OUT_OBJECT,
                IID_PPV_ARGS(shader.ReleaseAndGetAddressOf()),
                nullptr),
            "get DXC shader object");
        return shader;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> compile_shader(
        const std::wstring& path,
        const char* target,
        const char* entry_point,
        const util::ProgramArgument& args) {

        ProgramArgumentDefines define_storage(args);
        return compile_shader(
            path,
            target,
            entry_point,
            define_storage.defines.data());
    }
}
