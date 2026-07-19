#pragma once

#include <d3d12.h>

namespace rndr {

    enum class EnumRootParamScene : UINT {
        FRAME_CONSTANT = 0,          // b0, space0
        DRAW_CONSTANT = 1,           // b1, space0
        INSTANCE_BUFFER = 2,         // t0, space1
        MATERIAL_BUFFER = 3,         // t1, space1
        GEOMETRY_BUFFER = 4,         // t2-t4, space1
        MATERIAL_TEXTURE = 5,        // t0+, space2
        MATERIAL_SAMPLER = 6,        // s0, space2

        BENCH_INSTANCE_BUFFER = 7,   // t0, space0
        BENCH_MATERIAL_BUFFER = 8,   // t1, space0
        BENCH_MATERIAL_TEXTURE = 9,  // t8+, space0
        BENCH_MATERIAL_SAMPLER = 10, // s0, space0
        COUNT
    };

    enum class EnumRootParamFullscreen : UINT {
        PASS_CONSTANT = 0,           // b0, space0
        INPUT_SRV = 1,               // t0+, space0
        SAMPLER = 2,                 // s0+, space0

        BENCH_INPUT_SRV = 3,         // benchmark-specific SRV table
        BENCH_MATERIAL_TEXTURE = 4,  // t8+, space0
        BENCH_MATERIAL_SAMPLER = 5,  // s0, space0
        COUNT
    };

    enum class EnumRootParamCompute : UINT {
        PASS_CONSTANT = 0,           // b0, space0
        INPUT_SRV = 1,               // t0+, space0
        OUTPUT_UAV = 2,              // u0+, space0
        SAMPLER = 3,                 // s0+, space0
        COUNT
    };

    enum class EnumRootParamDeferredLighting : UINT {
        PASS_CONSTANT = 0,           // b0, space0
        LIGHTING_INPUT = 1,          // t0-t3, space0
        GBUFFER_INPUT = 2,           // t8-t17, space0
        OUTPUT = 3,                  // u0, space0
        SAMPLER = 4,                 // s0-s3, space0
        COUNT
    };

    template<typename T>
    [[nodiscard]] constexpr UINT root_param(T parameter) {
        return static_cast<UINT>(parameter);
    }

}
