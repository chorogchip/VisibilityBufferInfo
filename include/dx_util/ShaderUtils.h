#pragma once

#include <Windows.h>
#include <wrl.h>
#include <d3dcompiler.h>
#include <string>

namespace dxutl {

    Microsoft::WRL::ComPtr<ID3DBlob> compile_shader(
        const std::wstring& path, const char* target, const char* entry_point = "main",
        const D3D_SHADER_MACRO* defines = nullptr);

}
