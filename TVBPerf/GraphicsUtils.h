#pragma once

#include <Windows.h>
#include <d3d12.h>

class GraphicsUtils {
public:
	static void compile_shader(ID3DBlob** pp_blob, LPCWSTR filename, LPCSTR shader_model);
};