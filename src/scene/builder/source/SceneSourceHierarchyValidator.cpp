#include "scene/builder/source/SceneSourceHierarchyValidator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "util/Logger.h"

namespace scene {

    void SceneSourceHierarchyValidator::validate(
        const source::Node& node) {

        const float* local_values = &node.local_transform.m[0][0];
        util::Logger::g_logger.assert_with_log(
            std::all_of(
                local_values,
                local_values + 16,
                [](float value) { return std::isfinite(value); }),
            "Scene source node has a non-finite local transform.");
        util::Logger::g_logger.assert_with_log(
            node.instance_count == 0 ||
            node.mesh_id != source::SceneConstants::INVALID_INDEX,
            "Scene source node has instances but no mesh.");
    }

    void SceneSourceHierarchyValidator::validate(
        const SceneSourceData& scene) {

        util::Logger::g_logger.assert_with_log(
            scene.root_node_id < scene.nodes.size(),
            "Scene source has an invalid root node.");

        std::vector<uint8_t> visited(scene.nodes.size(), 0);
        std::vector<uint32_t> parent_counts(scene.nodes.size(), 0);
        std::vector<uint32_t> stack = { scene.root_node_id };
        while (!stack.empty()) {
            const uint32_t node_id = stack.back();
            stack.pop_back();

            util::Logger::g_logger.assert_with_log(
                node_id < scene.nodes.size(),
                "Scene source hierarchy contains an invalid node ID.");
            util::Logger::g_logger.assert_with_log(
                visited[node_id] == 0,
                "Scene source hierarchy contains a cycle.");
            visited[node_id] = 1;

            for (uint32_t child_id : scene.nodes[node_id].children) {
                util::Logger::g_logger.assert_with_log(
                    child_id < scene.nodes.size() &&
                    child_id != node_id,
                    "Scene source hierarchy contains an invalid child ID.");
                ++parent_counts[child_id];
                util::Logger::g_logger.assert_with_log(
                    parent_counts[child_id] == 1,
                    "Scene source node has more than one parent.");
                stack.push_back(child_id);
            }
        }

        util::Logger::g_logger.assert_with_log(
            std::all_of(
                visited.begin(),
                visited.end(),
                [](uint8_t value) { return value == 1; }),
            "Scene source contains a node outside the root hierarchy.");
    }
}
