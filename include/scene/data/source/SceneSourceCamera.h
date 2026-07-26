#pragma once

#include <cstdint>

namespace scene::source {

    enum class CameraProjection : uint8_t {
        Perspective,
        Orthographic
    };

    struct Camera {
        CameraProjection projection = CameraProjection::Perspective;
        float vertical_fov = 0.0f;
        float aspect_ratio = 0.0f;
        float near_plane = 0.1f;
        float far_plane = 0.0f;
        float horizontal_magnification = 0.0f;
        float vertical_magnification = 0.0f;
    };
}
