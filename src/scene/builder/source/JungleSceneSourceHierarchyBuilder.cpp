#include "JungleSceneSourceBuilderInternal.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

#include <fastgltf/dxmath_element_traits.hpp>
#include <fastgltf/tools.hpp>

#include "util/Logger.h"

namespace scene::source::jungle {

    namespace {

        bool finite(const DirectX::XMFLOAT3& value) {
            return std::isfinite(value.x) &&
                std::isfinite(value.y) &&
                std::isfinite(value.z);
        }

        bool finite(const DirectX::XMFLOAT4& value) {
            return std::isfinite(value.x) &&
                std::isfinite(value.y) &&
                std::isfinite(value.z) &&
                std::isfinite(value.w);
        }

        NodeKind kind_from_name(const fastgltf::Node& node) {
            const std::string_view name(
                node.name.data(),
                node.name.size());
            if (name.starts_with("JR_ROOT")) {
                return NodeKind::SceneRoot;
            }
            if (name.starts_with("JR_REGION__")) {
                return NodeKind::Region;
            }
            if (name.starts_with("JR_CELL__")) {
                return NodeKind::Cell;
            }
            if (name.starts_with("JR_SYSTEM__")) {
                return NodeKind::System;
            }
            if (name.starts_with("JR_INST__")) {
                return NodeKind::InstanceSet;
            }
            return NodeKind::Generic;
        }

        void append_instances(
            const fastgltf::Asset& asset,
            const fastgltf::Node& source_node,
            SceneSourceData& scene,
            Node& node) {

            if (source_node.instancingAttributes.empty()) {
                return;
            }

            const fastgltf::Attribute& first_attribute =
                source_node.instancingAttributes.front();
            util::Logger::g_logger.assert_with_log(
                first_attribute.accessorIndex < asset.accessors.size(),
                "glTF instancing attribute references an invalid accessor.");

            const size_t instance_count =
                asset.accessors[first_attribute.accessorIndex].count;
            node.first_instance = to_uint32(
                scene.instances.size(),
                "Scene instance offset exceeds 32-bit indexing.");
            node.instance_count = to_uint32(
                instance_count,
                "Scene instance count exceeds 32-bit indexing.");
            scene.instances.resize(
                scene.instances.size() + instance_count);
            for (uint32_t index = 0; index < node.instance_count; ++index) {
                scene.instances[node.first_instance + index].
                    source_index = index;
            }

            bool values_valid = true;
            const auto translation_it =
                source_node.findInstancingAttribute("TRANSLATION");
            if (translation_it !=
                source_node.instancingAttributes.end()) {
                util::Logger::g_logger.assert_with_log(
                    translation_it->accessorIndex <
                    asset.accessors.size(),
                    "glTF instancing translation references an invalid accessor.");
                const fastgltf::Accessor& accessor =
                    asset.accessors[
                        translation_it->accessorIndex];
                util::Logger::g_logger.assert_with_log(
                    accessor.count == instance_count,
                    "glTF instancing translation count differs.");
                fastgltf::iterateAccessorWithIndex<
                    DirectX::XMFLOAT3>(
                    asset,
                    accessor,
                    [&](DirectX::XMFLOAT3 value, size_t index) {
                        value.z = -value.z;
                        values_valid = values_valid && finite(value);
                        scene.instances[
                            node.first_instance +
                            static_cast<uint32_t>(index)].
                            translation = value;
                    });
            }

            const auto rotation_it =
                source_node.findInstancingAttribute("ROTATION");
            if (rotation_it !=
                source_node.instancingAttributes.end()) {
                util::Logger::g_logger.assert_with_log(
                    rotation_it->accessorIndex <
                    asset.accessors.size(),
                    "glTF instancing rotation references an invalid accessor.");
                const fastgltf::Accessor& accessor =
                    asset.accessors[rotation_it->accessorIndex];
                util::Logger::g_logger.assert_with_log(
                    accessor.count == instance_count,
                    "glTF instancing rotation count differs.");
                fastgltf::iterateAccessorWithIndex<
                    DirectX::XMFLOAT4>(
                    asset,
                    accessor,
                    [&](DirectX::XMFLOAT4 value, size_t index) {
                        value.x = -value.x;
                        value.y = -value.y;
                        values_valid = values_valid && finite(value);
                        scene.instances[
                            node.first_instance +
                            static_cast<uint32_t>(index)].
                            rotation = value;
                    });
            }

            const auto scale_it =
                source_node.findInstancingAttribute("SCALE");
            if (scale_it !=
                source_node.instancingAttributes.end()) {
                util::Logger::g_logger.assert_with_log(
                    scale_it->accessorIndex <
                    asset.accessors.size(),
                    "glTF instancing scale references an invalid accessor.");
                const fastgltf::Accessor& accessor =
                    asset.accessors[scale_it->accessorIndex];
                util::Logger::g_logger.assert_with_log(
                    accessor.count == instance_count,
                    "glTF instancing scale count differs.");
                fastgltf::iterateAccessorWithIndex<
                    DirectX::XMFLOAT3>(
                    asset,
                    accessor,
                    [&](DirectX::XMFLOAT3 value, size_t index) {
                        values_valid = values_valid && finite(value);
                        scene.instances[
                            node.first_instance +
                            static_cast<uint32_t>(index)].
                            scale = value;
                    });
            }

            const auto source_index_it =
                source_node.findInstancingAttribute(
                    "_JR_SOURCE_INDEX");
            if (source_index_it !=
                source_node.instancingAttributes.end()) {
                util::Logger::g_logger.assert_with_log(
                    source_index_it->accessorIndex <
                    asset.accessors.size(),
                    "Jungle source-index references an invalid accessor.");
                const fastgltf::Accessor& accessor =
                    asset.accessors[
                        source_index_it->accessorIndex];
                util::Logger::g_logger.assert_with_log(
                    accessor.count == instance_count,
                    "Jungle source-index count differs.");
                std::vector<uint32_t> indices(instance_count);
                fastgltf::copyFromAccessor<uint32_t>(
                    asset,
                    accessor,
                    indices.data());
                for (size_t index = 0; index < indices.size(); ++index) {
                    scene.instances[
                        node.first_instance +
                        static_cast<uint32_t>(index)].
                        source_index = indices[index];
                }
            }

            util::Logger::g_logger.assert_with_log(
                values_valid,
                "glTF instancing stream contains a non-finite value.");
        }

        void visit_node(
            const Context& context,
            const fastgltf::Asset& asset,
            size_t source_node_id,
            uint32_t parent_id,
            const std::vector<uint32_t>& mesh_ids,
            SceneSourceData& scene) {

            util::Logger::g_logger.assert_with_log(
                source_node_id < asset.nodes.size(),
                "glTF hierarchy references an invalid node.");

            const fastgltf::Node& source_node =
                asset.nodes[source_node_id];
            Node node{};
            node.name.assign(
                source_node.name.data(),
                source_node.name.size());
            DirectX::XMStoreFloat4x4(
                &node.local_transform,
                read_local_transform(source_node));

            if (source_node.meshIndex &&
                *source_node.meshIndex < mesh_ids.size()) {
                node.mesh_id = mesh_ids[*source_node.meshIndex];
            }
            if (source_node.cameraIndex &&
                *source_node.cameraIndex < scene.cameras.size()) {
                node.camera_id = to_uint32(
                    *source_node.cameraIndex,
                    "glTF camera index exceeds 32-bit indexing.");
            }

            if (source_node_id < context.node_metadata.size()) {
                const NodeMetadata& metadata =
                    context.node_metadata[source_node_id];
                node.kind = metadata.kind;
                node.region = metadata.region;
                node.world_bounds = metadata.world_bounds;
                node.stable_id = metadata.stable_id;
                node.jungle = metadata.jungle;
            }
            if (node.kind == NodeKind::Generic) {
                node.kind = kind_from_name(source_node);
            }

            append_instances(
                asset,
                source_node,
                scene,
                node);

            const uint32_t node_id = to_uint32(
                scene.nodes.size(),
                "Scene node count exceeds 32-bit indexing.");
            scene.nodes.emplace_back(std::move(node));
            scene.nodes[parent_id].children.push_back(node_id);

            for (size_t child_id : source_node.children) {
                visit_node(
                    context,
                    asset,
                    child_id,
                    node_id,
                    mesh_ids,
                    scene);
            }
        }
    }

    void append_hierarchy(
        const Context& context,
        const fastgltf::Asset& asset,
        const std::vector<uint32_t>& mesh_ids,
        SceneSourceData& scene) {

        Node root{};
        root.name = "JR_LOADER_ROOT";
        root.kind = NodeKind::SceneRoot;
        root.stable_id = "jr:loader:root";
        scene.root_node_id = 0;
        scene.nodes.emplace_back(std::move(root));

        util::Logger::g_logger.assert_with_log(
            !asset.scenes.empty(),
            "Jungle GLB has no scene.");

        const size_t scene_index =
            asset.defaultScene &&
            *asset.defaultScene < asset.scenes.size()
            ? *asset.defaultScene
            : 0;
        for (size_t source_node_id : asset.scenes[scene_index].nodeIndices) {
            visit_node(
                context,
                asset,
                source_node_id,
                scene.root_node_id,
                mesh_ids,
                scene);
        }

        util::Logger::g_logger.assert_with_log(
            scene.nodes.size() != 1,
            "Jungle GLB scene has no root nodes.");
    }
}
