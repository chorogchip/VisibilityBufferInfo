#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "math/AABB.h"
#include "scene/data/source/SceneConstants.h"
#include "scene/data/source/SceneSourceSemantic.h"

namespace scene::source {

    enum class NodeKind : uint8_t {
        Generic,
        SceneRoot,
        Region,
        Cell,
        System,
        InstanceSet,
        StaticObject,
        Camera,
        UnresolvedContainer,
        StaticContainer,
        TerrainContainer,
        SystemContainer,
        PrototypeContainer
    };

    enum class Region : uint8_t {
        None,
        Global,
        Cinematic,
        Extended,
        Pyramid
    };

    enum class Provenance : uint8_t {
        None,
        Source,
        Computed,
        Inferred
    };

    enum class UnresolvedReason : uint8_t {
        None,
        ExactOrigin,
        OutsideCellOwnership
    };

    struct JungleNodeMetadata {
        std::string cell;
        std::string system;
        std::string species;
        std::string prototype_name;
        std::string prototype_id;
        std::string source_object;
        std::string source_prim;
        std::string source_layer;
        Provenance provenance = Provenance::None;
        UnresolvedReason unresolved_reason = UnresolvedReason::None;
    };

    // Compact glTF TRS plus the reversible source-array index used by the
    // Jungle USD point-instancer pipeline.
    struct InstanceTransform {
        DirectX::XMFLOAT3 translation = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
        uint32_t source_index = SceneConstants::INVALID_INDEX;
    };

    static_assert(sizeof(InstanceTransform) == 44);

    // Matrices use DirectX row-vector order and are not transposed for shaders.
    // An instance range belongs to this node and uses node-local transforms.
    struct Node {
        std::string name;
        std::vector<uint32_t> children;
        DirectX::XMFLOAT4X4 local_transform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        uint32_t mesh_id = SceneConstants::INVALID_INDEX;
        uint32_t polygon_mesh_id = SceneConstants::INVALID_INDEX;
        uint32_t camera_id = SceneConstants::INVALID_INDEX;
        uint32_t first_instance = 0;
        uint32_t instance_count = 0;

        NodeKind kind = NodeKind::Generic;
        Region region = Region::None;
        math::AABB world_bounds{};
        std::string stable_id;
        JungleNodeMetadata jungle;

        SourceReference source;
        uint32_t parent_node_id = SceneConstants::INVALID_INDEX;
        DirectX::XMFLOAT4X4 world_transform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        std::string prim_type;
        std::vector<std::string> applied_schemas;
        std::string purpose;
        bool visible = true;
        bool active = true;
        bool loaded = true;
        bool defined = true;
        bool abstract = false;
        bool reset_xform_stack = false;
        bool native_instance = false;

    };
}
