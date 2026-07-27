#include "dx_util/ShaderUtils.h"

#include <string>
#include <vector>

#include "util/Logger.h"
#include "util/Utils.h"

namespace dxutl {

    Microsoft::WRL::ComPtr<IDxcBlob> compile_shader(
        const std::wstring& path,
        const wchar_t* target,
        const wchar_t* entry_point,
        const std::vector<std::wstring>& defines) {

        util::Logger::g_logger.assert_with_log(
            target != nullptr && entry_point != nullptr,
            "shader target and entry point must be valid");

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

        std::vector<LPCWSTR> arguments = {
            path.c_str(),
            L"-E",
            entry_point,
            L"-T",
            target
        };

#if defined(_DEBUG)
        arguments.push_back(L"-Zi");
        arguments.push_back(L"-Od");
        arguments.push_back(L"-Qembed_debug");
#else
        arguments.push_back(L"-O3");
#endif

        for (const std::wstring& define_argument : defines) {
            arguments.push_back(L"-D");
            arguments.push_back(define_argument.c_str());
        }

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

        HRESULT status = S_OK;
        util::Utils::throw_if_failed(
            result->GetStatus(&status),
            "get DXC compile status");
        if (FAILED(status)) {
            util::Logger::g_logger
                << "DXC compile failed for "
                << util::Utils::wstring_to_string(path)
                << "\n";

            if (errors && errors->GetStringLength() > 0) {
                util::Logger::g_logger
                    << "DXC diagnostics:\n"
                    << errors->GetStringPointer();
            } else {
                util::Logger::g_logger
                    << "DXC returned no diagnostic text.\n";
            }

            util::Logger::g_logger.assert_with_log(false, "compile shader");
        }

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
        const wchar_t* target,
        const wchar_t* entry_point,
        const util::ProgramArgument& args) {

        const std::vector<std::wstring> defines = {
            std::wstring(L"GBUFFER_COUNT=") + std::to_wstring(args.gbuffer_cnt),
            std::wstring(L"TEXTURE_COUNT=") + std::to_wstring(args.texture_count),
            std::wstring(L"TEXTURE_SAMPLING_COUNT=") +
                std::to_wstring(args.texture_sampling_count),
            std::wstring(L"TEXTURE_SIZE=") + std::to_wstring(args.texture_size),
            std::wstring(L"ALU_CALC_COUNT=") + std::to_wstring(args.alu_calc_count)
        };
        return compile_shader(
            path,
            target,
            entry_point,
            defines);
    }
}
