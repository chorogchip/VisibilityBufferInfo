#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <fastgltf/core.hpp>

#include "scene/data/source/SceneSourceData.h"

namespace scene::source::jungle {

    // Shared state for the Jungle source builder implementation files.
    struct NodeMetadata {
        NodeKind kind = NodeKind::Generic;
        Region region = Region::None;
        math::AABB world_bounds{};
        std::string stable_id;
        JungleNodeMetadata jungle;
    };

    struct FileLayout {
        uint64_t binary_offset = 0;
        uint64_t binary_size = 0;
    };

    struct Context {
        std::filesystem::path source_path;
        FileLayout file_layout;
        std::vector<NodeMetadata> node_metadata;
    };

    fastgltf::Asset load_asset(Context& context);

    void append_materials(
        const Context& context,
        const fastgltf::Asset& asset,
        SceneSourceData& scene);

    void append_geometry(
        const fastgltf::Asset& asset,
        SceneSourceData& scene,
        std::vector<uint32_t>& mesh_ids);

    void append_cameras(
        const fastgltf::Asset& asset,
        SceneSourceData& scene);

    void append_hierarchy(
        const Context& context,
        const fastgltf::Asset& asset,
        const std::vector<uint32_t>& mesh_ids,
        SceneSourceData& scene);

    uint32_t to_uint32(size_t value, const char* message);
    DirectX::XMMATRIX read_local_transform(const fastgltf::Node& node);
}
