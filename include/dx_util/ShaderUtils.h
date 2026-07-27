#pragma once

#include <Windows.h>
#include <wrl.h>
#include <dxcapi.h>
#include <string>
#include <vector>

#include "ProgramArgument.h"

namespace dxutl {

    Microsoft::WRL::ComPtr<IDxcBlob> compile_shader(
        const std::wstring& path, const wchar_t* target, const wchar_t* entry_point,
        const std::vector<std::wstring>& defines);

    Microsoft::WRL::ComPtr<IDxcBlob> compile_shader(
        const std::wstring& path, const wchar_t* target, const wchar_t* entry_point,
        const util::ProgramArgument& args = {});
}
