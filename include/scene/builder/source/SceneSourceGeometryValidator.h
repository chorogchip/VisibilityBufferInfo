#pragma once

#include "scene/data/source/SceneSourceGeometry.h"

namespace scene {

    class SceneSourceGeometryValidator {
    public:
        static void validate(const source::Primitive& primitive);
        static void validate(const source::Mesh& mesh);
    };
}
