#pragma once

#include <Windows.h>
#include <wrl.h>
#include <d3dcompiler.h>
#include <string>

#include "ProgramArgument.h"

namespace dxutl {

    Microsoft::WRL::ComPtr<ID3DBlob> compile_shader(
        const std::wstring& path, const char* target, const char* entry_point,
        const D3D_SHADER_MACRO* defines);

    Microsoft::WRL::ComPtr<ID3DBlob> compile_shader(
        const std::wstring& path, const char* target, const char* entry_point,
        const util::ProgramArgument& args = {});
}
