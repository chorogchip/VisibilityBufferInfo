#pragma once

#include <cstdint>
#include <vector>

#include "scene/data/source/SceneConstants.h"
#include "scene/data/source/SceneSourceGeometry.h"
#include "scene/data/source/SceneSourceHierarchy.h"
#include "scene/data/source/SceneSourceMaterial.h"

namespace scene {

    // Renderer-independent scene data decoded from a source file or generated
    // synthetically. Every loader must normalize coordinates and units before
    // constructing this triangle-only, indexed representation. It intentionally
    // contains no derived world transforms, render batches, GPU layouts, or
    // decoded texture pixels.
    struct SceneSourceData {
        uint32_t root_node_id = source::SceneConstants::INVALID_INDEX;
        std::vector<source::Node> nodes;
        std::vector<source::Mesh> meshes;
        std::vector<source::Material> materials;

        void validate() const;

    private:
        void validate_hierarchy_() const;
        void validate_references_() const;
    };
}
