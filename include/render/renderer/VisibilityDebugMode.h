#pragma once

#include <cstdint>

namespace rndr {

    enum class VisibilityDebugMode : std::uint32_t {
        GeometryInstanceHash = 0,
        PrimitiveHash,
        GeometryPrimitiveHash,
        Barycentric,
        PerspectiveBarycentric,
        BarycentricDx,
        BarycentricDy,
        UvDx,
        UvDy,
        UvLodProxy,
        MaterialIdHash,
        MaterialBinHash,
    };

}
