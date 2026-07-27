#pragma once

#include <Windows.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <dxcapi.h>
#include <string>

#include "ProgramArgument.h"

namespace dxutl {

    Microsoft::WRL::ComPtr<IDxcBlob> compile_shader(
        const std::wstring& path, const char* target, const char* entry_point,
        const D3D_SHADER_MACRO* defines);

    Microsoft::WRL::ComPtr<IDxcBlob> compile_shader(
        const std::wstring& path, const char* target, const char* entry_point,
        const util::ProgramArgument& args = {});
}
