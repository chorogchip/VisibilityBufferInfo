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
            nullptr,
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

}
