#include "render/pass/PassDonutDeferredLighting.h"

#include "dx_util/ResourceUtils.h"
#include "dx_util/ShaderUtils.h"
#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerShader.h"
#include "engine/RootSignatureBuilder.h"

namespace rndr {

    void PassDonutDeferredLighting::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutDeferredLightingResources& resources) {

        resources_ = resources;
    }

    void PassDonutDeferredLighting::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        UINT width,
        UINT height) {


    }

}
