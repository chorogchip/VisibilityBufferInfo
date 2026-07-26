#include "scene/data/source/SceneSourceGeometry.h"

#include "util/Logger.h"

namespace scene::source {

    void Primitive::validate() const {
        util::Logger::g_logger.assert_with_log(
            !positions.empty(),
            "Scene source primitive has no positions.");
        util::Logger::g_logger.assert_with_log(
            normals.empty() || normals.size() == positions.size(),
            "Scene source primitive normal count differs from its position count.");
        util::Logger::g_logger.assert_with_log(
            tangents.empty() || tangents.size() == positions.size(),
            "Scene source primitive tangent count differs from its position count.");
        util::Logger::g_logger.assert_with_log(
            uv0.empty() || uv0.size() == positions.size(),
            "Scene source primitive UV count differs from its position count.");
        util::Logger::g_logger.assert_with_log(
            !indices.empty() && indices.size() % 3 == 0,
            "Scene source primitive must contain indexed triangles.");

        for (uint32_t index : indices) {
            util::Logger::g_logger.assert_with_log(
                index < positions.size(),
                "Scene source primitive contains an out-of-range index.");
        }
    }

    void Mesh::validate() const {
        util::Logger::g_logger.assert_with_log(
            !primitives.empty(),
            "Scene source mesh has no primitives.");

        for (const Primitive& primitive : primitives) {
            primitive.validate();
        }
    }
}
