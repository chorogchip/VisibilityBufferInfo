#include "JungleSceneSourceBuilderInternal.h"

#include <variant>

namespace scene::source::jungle {

    void append_cameras(
        const fastgltf::Asset& asset,
        SceneSourceData& scene) {

        scene.cameras.reserve(asset.cameras.size());
        for (const fastgltf::Camera& source : asset.cameras) {
            Camera camera{};
            if (const auto* perspective =
                std::get_if<fastgltf::Camera::Perspective>(
                    &source.camera)) {
                camera.projection = CameraProjection::Perspective;
                camera.vertical_fov = perspective->yfov;
                camera.aspect_ratio = perspective->aspectRatio
                    ? *perspective->aspectRatio
                    : 0.0f;
                camera.near_plane = perspective->znear;
                camera.far_plane = perspective->zfar
                    ? *perspective->zfar
                    : 0.0f;
            } else {
                const auto& orthographic =
                    std::get<fastgltf::Camera::Orthographic>(
                        source.camera);
                camera.projection = CameraProjection::Orthographic;
                camera.horizontal_magnification =
                    orthographic.xmag;
                camera.vertical_magnification =
                    orthographic.ymag;
                camera.near_plane = orthographic.znear;
                camera.far_plane = orthographic.zfar;
            }
            scene.cameras.emplace_back(camera);
        }
    }
}
