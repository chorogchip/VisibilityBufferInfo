#pragma once

#include <cstdint>
#include <vector>

#include "scene/data/source/SceneConstants.h"
#include "scene/data/source/SceneSourceCamera.h"
#include "scene/data/source/SceneSourceGeometry.h"
#include "scene/data/source/SceneSourceHierarchy.h"
#include "scene/data/source/SceneSourceMaterial.h"
#include "scene/data/source/SceneSourceSemantic.h"
#include "scene/data/source/SceneSourceTexture.h"

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
        std::vector<source::Camera> cameras;
        std::vector<source::Image> images;
        std::vector<source::Sampler> samplers;
        std::vector<source::Texture> textures;
        std::vector<source::InstanceTransform> instances;

        source::SceneMetadata metadata;
        std::vector<source::PolygonMesh> polygon_meshes;
        std::vector<source::NativePrototype> native_prototypes;
        std::vector<source::NativeInstance> native_instances;
        std::vector<source::PointInstancer> point_instancers;
        std::vector<source::MaterialGraph> material_graphs;
        std::vector<source::ShaderNode> shader_nodes;
        std::vector<source::MaterialBinding> material_bindings;
        std::vector<source::SourceCamera> source_cameras;
        std::vector<source::SourceLight> source_lights;
        std::vector<source::SourceAsset> source_assets;
        std::vector<source::ConversionDiagnostic> conversion_diagnostics;


        uint32_t active_material_class_count = 0;
    };
}
