#include "dx_util/ShaderUtils.h"

#include "util/Utils.h"

namespace dxutl {

    Microsoft::WRL::ComPtr<ID3DBlob> compile_shader(
        const std::wstring& path, const char* target,
        const char* entry_point, const D3D_SHADER_MACRO* defines) {

        Microsoft::WRL::ComPtr<ID3DBlob> shader;
        Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

        UINT compile_flags = 0;

#if defined(_DEBUG)
        compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        HRESULT hr = D3DCompileFromFile(
            path.c_str(),
            defines,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entry_point,
            target,
            compile_flags,
            0,
            shader.ReleaseAndGetAddressOf(),
            error_blob.ReleaseAndGetAddressOf());

        if (FAILED(hr)) {
            if (error_blob) {
                OutputDebugStringA(static_cast<const char*>(error_blob->GetBufferPointer()));
            }
            Utils::throw_if_failed(hr, "compile shader");
        }

        return shader;
    }


    Microsoft::WRL::ComPtr<ID3DBlob> compile_shader(
        const std::wstring& path, const char* target,
        const char* entry_point, const util::ProgramArgument& args) {

        std::string gbuffer_count_define = std::to_string(args.gbuffer_cnt);
        std::string texture_count_define = std::to_string(args.texture_count);
        std::string texture_sampling_count_define = std::to_string(args.texture_sampling_count);
        std::string texture_size_define = std::to_string(args.texture_size);
        std::string alu_calc_count_define = std::to_string(args.alu_calc_count);

        D3D_SHADER_MACRO defines[] =
        {
            { "GBUFFER_COUNT", gbuffer_count_define.c_str() },
            { "TEXTURE_COUNT", texture_count_define.c_str() },
            { "TEXTURE_SAMPLING_COUNT", texture_sampling_count_define.c_str() },
            { "TEXTURE_SIZE", texture_size_define.c_str() },
            { "ALU_CALC_COUNT", alu_calc_count_define.c_str() },
            { nullptr, nullptr }
        };

        return compile_shader(
            path, target, entry_point,
            defines
        );
    }
}
