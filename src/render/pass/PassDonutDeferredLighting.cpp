#include "render/pass/PassDonutDeferredLighting.h"

namespace rndr {

    void PassDonutDeferredLighting::init(
        ID3D12Device* device,
        const util::ProgramArgument& arguments,
        const PassDonutDeferredLightingResources& resources) {
        (void)device;
        (void)arguments;
        resources_ = resources;
    }

    void PassDonutDeferredLighting::render(
        ID3D12GraphicsCommandList* command_list,
        UINT frame_index,
        UINT width,
        UINT height) {
        (void)command_list;
        (void)frame_index;
        (void)width;
        (void)height;
    }

}
