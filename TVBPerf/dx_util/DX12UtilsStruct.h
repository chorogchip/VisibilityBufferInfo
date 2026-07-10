#pragma ocne

#include <d3d12.h>

namespace dxutl {
    D3D12_VIEWPORT create_viewport(float width, float height);
    D3D12_RECT create_scissor_rect(LONG width, LONG height);
}