#include "render/renderer/donut/RendererDonut.h"

#include "dx_util/ResourceUtils.h"
#include "util/RenderConstants.h"

namespace rndr {

    RendererDonut::~RendererDonut() = default;


    void RendererDonut::init1_() {
        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = util::RenderConstants::DONUT_DEPTH_DSV_FORMAT;
        clear_value.DepthStencil.Depth = 1.f;

        depth_stencil_buffer_.init(
            dxutl::create_texture2d(
                device_.Get(),
                width_,
                height_,
                util::RenderConstants::DONUT_DEPTH_RESOURCE_FORMAT,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
                &clear_value).Get(),
            D3D12_RESOURCE_STATE_DEPTH_WRITE);

        /*
        resource_manager_shader_.init(
            device_.Get(),
            static_cast<UINT>(eng::ResourceManagerShader::EnumDescPos::COUNT));*/

        this->init2_();
    }

    void RendererDonut::render_prepare_() {

    }

}
