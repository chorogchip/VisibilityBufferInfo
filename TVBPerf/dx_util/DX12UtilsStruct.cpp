#include "DX12UtilsStruct.h"

namespace dxutl {

	D3D12_VIEWPORT create_viewport(float width, float height) {
        
        D3D12_VIEWPORT ret{};
        ret.TopLeftX = 0.0f;
        ret.TopLeftY = 0.0f;
        ret.Width = width;
        ret.Height = height;
        ret.MinDepth = 0.0f;
        ret.MaxDepth = 1.0f;

        return ret;
	}

	D3D12_RECT create_scissor_rect(LONG width, LONG height) {

        D3D12_RECT ret{};
        ret.left = 0;
        ret.top = 0;
        ret.right = width;
        ret.bottom = height;

        return ret;
	}
}