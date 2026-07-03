#include "GraphicsUtils.h"

#include <wrl.h>
#include <d3dcompiler.h>

#include "Utils.h"

void GraphicsUtils::compile_shader(ID3DBlob** pp_blob, LPCWSTR filename, LPCSTR shader_model) {
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

    UINT compileFlags = 0;

#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(
        filename, nullptr, nullptr, "main", shader_model,
        compileFlags, 0, pp_blob, &error_blob
    );

    if (FAILED(hr))
    {
        if (error_blob)
        {
            OutputDebugStringA(static_cast<const char*>(error_blob->GetBufferPointer()));
        }
        Utils::throw_if_failed(hr, "compile shader");
    }
}