#include "scene/data/source/SceneSourceHierarchy.h"

#include <algorithm>
#include <cmath>

#include "util/Logger.h"

namespace scene::source {

    void Node::validate() const {
        const float* local_values = &local_transform.m[0][0];
        util::Logger::g_logger.assert_with_log(
            std::all_of(
                local_values,
                local_values + 16,
                [](float value) { return std::isfinite(value); }),
            "Scene source node has a non-finite local transform.");
        util::Logger::g_logger.assert_with_log(
            instance_transforms.empty() ||
            mesh_id != SceneConstants::INVALID_INDEX,
            "Scene source node has instance transforms but no mesh.");

        for (const DirectX::XMFLOAT4X4& transform : instance_transforms) {
            const float* values = &transform.m[0][0];
            util::Logger::g_logger.assert_with_log(
                std::all_of(
                    values,
                    values + 16,
                    [](float value) { return std::isfinite(value); }),
                "Scene source node has a non-finite instance transform.");
        }
    }
}
