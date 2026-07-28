#include "dx_util/ShaderUtils.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "util/Logger.h"
#include "util/Utils.h"

namespace {

    std::string format_hresult(HRESULT result) {
        std::ostringstream stream;
        stream
            << "0x"
            << std::uppercase
            << std::hex
            << static_cast<unsigned int>(result);
        return stream.str();
    }

    void log_shader_context(
        const std::wstring& path,
        const wchar_t* target,
        const wchar_t* entry_point,
        const std::vector<std::wstring>& defines) {

        const auto to_utf8 = [](const wchar_t* value) {
            return value != nullptr
                ? util::Utils::wstring_to_string(value)
                : std::string("(null)");
        };

        util::Logger::g_logger
            << "  Source: " << util::Utils::wstring_to_string(path) << '\n'
            << "  Target: " << to_utf8(target) << '\n'
            << "  Entry point: " << to_utf8(entry_point) << '\n';

        if (defines.empty()) {
            util::Logger::g_logger << "  Defines: (none)\n";
            return;
        }

        util::Logger::g_logger << "  Defines:\n";
        for (const std::wstring& define : defines) {
            util::Logger::g_logger
                << "    " << util::Utils::wstring_to_string(define) << '\n';
        }
    }

    [[noreturn]] void fail_shader_operation(
        const char* operation,
        HRESULT result,
        const std::wstring& path,
        const wchar_t* target,
        const wchar_t* entry_point,
        const std::vector<std::wstring>& defines,
        const IDxcBlobUtf8* diagnostics = nullptr) {

        util::Logger::g_logger
            << "Shader " << operation << " failed.\n"
            << "  HRESULT: " << format_hresult(result) << '\n';
        log_shader_context(path, target, entry_point, defines);

        if (diagnostics != nullptr && diagnostics->GetStringLength() > 0) {
            util::Logger::g_logger
                << "DXC diagnostics:\n"
                << diagnostics->GetStringPointer() << '\n';
        } else {
            util::Logger::g_logger << "DXC diagnostics: (none available)\n";
        }

        util::Logger::g_logger.assert_with_log(false, "shader compile or load failure");
    }
}

namespace dxutl {

    Microsoft::WRL::ComPtr<IDxcBlob> compile_shader(
        const std::wstring& path,
        const wchar_t* target,
        const wchar_t* entry_point,
        const std::vector<std::wstring>& defines) {

        if (target == nullptr || entry_point == nullptr) {
            util::Logger::g_logger << "Shader request is invalid.\n";
            log_shader_context(path, target, entry_point, defines);
            util::Logger::g_logger.assert_with_log(
                false,
                "shader target and entry point must be valid");
        }

        Microsoft::WRL::ComPtr<IDxcUtils> utils;
        HRESULT result_code = DxcCreateInstance(
            CLSID_DxcUtils,
            IID_PPV_ARGS(utils.ReleaseAndGetAddressOf()));
        if (FAILED(result_code)) {
            fail_shader_operation(
                "DXC utility creation",
                result_code,
                path,
                target,
                entry_point,
                defines);
        }

        Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
        result_code = DxcCreateInstance(
            CLSID_DxcCompiler,
            IID_PPV_ARGS(compiler.ReleaseAndGetAddressOf()));
        if (FAILED(result_code)) {
            fail_shader_operation(
                "DXC compiler creation",
                result_code,
                path,
                target,
                entry_point,
                defines);
        }

        Microsoft::WRL::ComPtr<IDxcIncludeHandler> include_handler;
        result_code = utils->CreateDefaultIncludeHandler(
            include_handler.ReleaseAndGetAddressOf());
        if (FAILED(result_code)) {
            fail_shader_operation(
                "DXC include handler creation",
                result_code,
                path,
                target,
                entry_point,
                defines);
        }

        Microsoft::WRL::ComPtr<IDxcBlobEncoding> source;
        result_code = utils->LoadFile(
            path.c_str(),
            nullptr,
            source.ReleaseAndGetAddressOf());
        if (FAILED(result_code)) {
            fail_shader_operation(
                "source load",
                result_code,
                path,
                target,
                entry_point,
                defines);
        }

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
        result_code = compiler->Compile(
            &source_buffer,
            arguments.data(),
            static_cast<UINT32>(arguments.size()),
            include_handler.Get(),
            IID_PPV_ARGS(result.ReleaseAndGetAddressOf()));
        if (FAILED(result_code)) {
            fail_shader_operation(
                "DXC compile invocation",
                result_code,
                path,
                target,
                entry_point,
                defines);
        }

        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
        result_code = result->GetOutput(
            DXC_OUT_ERRORS,
            IID_PPV_ARGS(errors.ReleaseAndGetAddressOf()),
            nullptr);
        if (FAILED(result_code)) {
            fail_shader_operation(
                "DXC diagnostic retrieval",
                result_code,
                path,
                target,
                entry_point,
                defines);
        }

        HRESULT status = S_OK;
        result_code = result->GetStatus(&status);
        if (FAILED(result_code)) {
            fail_shader_operation(
                "DXC compile status retrieval",
                result_code,
                path,
                target,
                entry_point,
                defines,
                errors.Get());
        }
        if (FAILED(status)) {
            fail_shader_operation(
                "compilation",
                status,
                path,
                target,
                entry_point,
                defines,
                errors.Get());
        }

        Microsoft::WRL::ComPtr<IDxcBlob> shader;
        result_code = result->GetOutput(
            DXC_OUT_OBJECT,
            IID_PPV_ARGS(shader.ReleaseAndGetAddressOf()),
            nullptr);
        if (FAILED(result_code)) {
            fail_shader_operation(
                "compiled shader object retrieval",
                result_code,
                path,
                target,
                entry_point,
                defines,
                errors.Get());
        }
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
