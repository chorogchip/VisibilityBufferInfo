#include "scene/builder/source/SceneSourceGeometryValidator.h"

#include "util/Logger.h"

namespace scene {

    void SceneSourceGeometryValidator::validate(
        const source::Primitive& primitive) {

        util::Logger::g_logger.assert_with_log(
            !primitive.positions.empty(),
            "Scene source primitive has no positions.");
        util::Logger::g_logger.assert_with_log(
            primitive.normals.empty() ||
            primitive.normals.size() == primitive.positions.size(),
            "Scene source primitive normal count differs from its position count.");
        util::Logger::g_logger.assert_with_log(
            primitive.tangents.empty() ||
            primitive.tangents.size() == primitive.positions.size(),
            "Scene source primitive tangent count differs from its position count.");
        util::Logger::g_logger.assert_with_log(
            primitive.uv0.empty() ||
            primitive.uv0.size() == primitive.positions.size(),
            "Scene source primitive UV count differs from its position count.");
        util::Logger::g_logger.assert_with_log(
            primitive.uv1.empty() ||
            primitive.uv1.size() == primitive.positions.size(),
            "Scene source primitive UV1 count differs from its position count.");
        util::Logger::g_logger.assert_with_log(
            primitive.color0.empty() ||
            primitive.color0.size() == primitive.positions.size(),
            "Scene source primitive color0 count differs from its position count.");
        util::Logger::g_logger.assert_with_log(
            primitive.color1.empty() ||
            primitive.color1.size() == primitive.positions.size(),
            "Scene source primitive color1 count differs from its position count.");
        util::Logger::g_logger.assert_with_log(
            !primitive.indices.empty() &&
            primitive.indices.size() % 3 == 0,
            "Scene source primitive must contain indexed triangles.");

        for (uint32_t index : primitive.indices) {
            util::Logger::g_logger.assert_with_log(
                index < primitive.positions.size(),
                "Scene source primitive contains an out-of-range index.");
        }
    }

    void SceneSourceGeometryValidator::validate(
        const source::Mesh& mesh) {

        util::Logger::g_logger.assert_with_log(
            !mesh.primitives.empty(),
            "Scene source mesh has no primitives.");
        for (const source::Primitive& primitive : mesh.primitives) {
            SceneSourceGeometryValidator::validate(primitive);
        }
    }
}
