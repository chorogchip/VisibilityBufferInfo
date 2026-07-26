#include "JungleSceneSourceValidationInternal.h"

#include <cmath>
#include <memory>
#include <ostream>
#include <string>

#include "scene/builder/source/JungleSceneSourceBuilder.h"

namespace jungle::validation {

    namespace {

        bool is_exact_origin(
            const scene::source::InstanceTransform& instance) {

            return instance.translation.x == 0.0f &&
                   instance.translation.y == 0.0f &&
                   instance.translation.z == 0.0f;
        }

        std::string source_key(
            const scene::source::JungleNodeMetadata& metadata) {

            std::string result = metadata.source_layer;
            result.push_back('\n');
            result += metadata.source_prim;
            return result;
        }

        bool append_instance_metadata(
            const scene::SceneSourceData& source_scene,
            const scene::source::Node& node,
            Totals& totals,
            SourceIndices& source_indices,
            std::string& error_message) {

            if (node.instance_count == 0) return true;

            ++totals.instance_sets;
            totals.instances += node.instance_count;
            if (node.kind != scene::source::NodeKind::InstanceSet) {
                error_message =
                    "A Jungle instanced node is not marked as an instance set.";
                return false;
            }
            if (node.jungle.source_layer.empty() ||
                node.jungle.source_prim.empty() ||
                node.jungle.prototype_id.empty() ||
                node.jungle.species.empty()) {
                error_message =
                    "An instance set lost required extras.jr identity.";
                return false;
            }
            if (node.jungle.provenance !=
                scene::source::Provenance::Computed) {
                error_message =
                    "An instance set lost its computed provenance.";
                return false;
            }

            auto& indices = source_indices[source_key(node.jungle)];
            indices.reserve(indices.size() + node.instance_count);
            const size_t first = node.first_instance;
            const size_t end = first + node.instance_count;
            for (size_t instance_id = first;
                 instance_id < end;
                 ++instance_id) {
                const scene::source::InstanceTransform& instance =
                    source_scene.instances[instance_id];
                indices.push_back(instance.source_index);
                if (is_exact_origin(instance)) {
                    ++totals.exact_origin_instances;
                }
            }

            if (node.jungle.unresolved_reason ==
                scene::source::UnresolvedReason::ExactOrigin) {
                totals.unresolved_exact_origin += node.instance_count;
            } else if (
                node.jungle.unresolved_reason ==
                scene::source::UnresolvedReason::OutsideCellOwnership) {
                totals.unresolved_outside_ownership +=
                    node.instance_count;
            }
            return true;
        }

        bool append_geometry(
            const scene::SceneSourceData& source_scene,
            Totals& totals,
            size_t& package_primitives,
            std::string& error_message) {

            for (const scene::source::Mesh& mesh : source_scene.meshes) {
                for (const scene::source::Primitive& primitive :
                     mesh.primitives) {
                    ++package_primitives;
                    if (primitive.positions.empty() ||
                        primitive.normals.empty() ||
                        primitive.uv0.empty() ||
                        primitive.indices.empty()) {
                        error_message =
                            "A Jungle primitive lost a required geometry stream.";
                        return false;
                    }
                    if (!primitive.uv1.empty()) {
                        ++totals.uv1_primitives;
                    }
                    if (!primitive.color0.empty()) {
                        ++totals.color0_primitives;
                    }
                    if (!primitive.color1.empty()) {
                        ++totals.color1_primitives;
                    }
                }
            }
            return true;
        }

        void append_materials(
            const scene::SceneSourceData& source_scene,
            Totals& totals) {

            for (const scene::source::Material& material :
                 source_scene.materials) {
                if (material.alpha_mode ==
                    scene::source::AlphaMode::Blend) {
                    ++totals.alpha_blend_materials;
                }
                if (material.transmission > 0.0f) {
                    ++totals.transmission_materials;
                }
                if (material.name == "River" &&
                    std::abs(material.specular_color.x - 2.666f) <
                        0.001f &&
                    std::abs(material.specular_color.y - 2.666f) <
                        0.001f &&
                    std::abs(material.specular_color.z - 2.666f) <
                        0.001f) {
                    totals.river_specular_color_preserved = true;
                }
            }
        }
    }

    bool append_package(
        const std::filesystem::path& path,
        Totals& totals,
        SourceIndices& source_indices,
        std::ostream& output,
        std::string& error_message) {

        output << "Loading " << path.filename().string()
               << "...\n" << std::flush;
        std::unique_ptr<scene::SceneSourceData> source_scene =
            scene::JungleSceneSourceBuilder::build(path);

        for (const scene::source::Node& node : source_scene->nodes) {
            if (node.kind == scene::source::NodeKind::Cell) {
                ++totals.cells;
            } else if (node.kind ==
                       scene::source::NodeKind::System) {
                ++totals.systems;
            }
            if (!append_instance_metadata(
                    *source_scene,
                    node,
                    totals,
                    source_indices,
                    error_message)) {
                return false;
            }
        }

        size_t package_primitives = 0;
        if (!append_geometry(
                *source_scene,
                totals,
                package_primitives,
                error_message)) {
            return false;
        }
        append_materials(*source_scene, totals);

        totals.meshes += source_scene->meshes.size();
        totals.primitives += package_primitives;
        totals.materials += source_scene->materials.size();
        totals.images += source_scene->images.size();
        totals.cameras += source_scene->cameras.size();

        output
            << "  nodes=" << source_scene->nodes.size()
            << ", meshes=" << source_scene->meshes.size()
            << ", primitives=" << package_primitives
            << ", instances=" << source_scene->instances.size()
            << '\n';
        return true;
    }
}
