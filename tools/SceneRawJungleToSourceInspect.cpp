#include "scene/builder/source/SceneRawJungleToSource.h"
#include "scene/raw/SceneRawJungle.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {

    const char* interpolation_name(scene::source::PrimvarInterpolation value) {
        using Interpolation = scene::source::PrimvarInterpolation;
        switch (value) {
        case Interpolation::Constant: return "constant";
        case Interpolation::Uniform: return "uniform";
        case Interpolation::Vertex: return "vertex";
        case Interpolation::Varying: return "varying";
        case Interpolation::FaceVarying: return "faceVarying";
        case Interpolation::Instance: return "instance";
        case Interpolation::Unknown: return "unknown";
        }
        return "unknown";
    }

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: SceneRawJungleToSourceInspect <JungleRuins_Karma.usda>\n";
        return 2;
    }

    try {
        const auto raw = scene::raw::SceneRawJungle::open(argv[1]);
        const auto source = scene::SceneRawJungleToSource::build(*raw);

        uint64_t polygon_count = 0;
        uint64_t face_vertex_count = 0;
        uint64_t primvar_count = 0;
        uint64_t logical_instance_count = 0;
        std::map<std::string, uint64_t> primvar_interpolations;
        for (const auto& mesh : source->polygon_meshes) {
            polygon_count += mesh.face_vertex_counts.size();
            face_vertex_count += mesh.face_vertex_indices.size();
            primvar_count += mesh.primvars.size();
            for (const auto& primvar : mesh.primvars) {
                ++primvar_interpolations[interpolation_name(primvar.interpolation)];
            }
        }
        for (const auto& point_instancer : source->point_instancers) {
            logical_instance_count += point_instancer.logical_instance_count;
        }

        uint64_t warnings = 0;
        uint64_t errors = 0;
        uint64_t fatals = 0;
        uint64_t unsupported = 0;
        uint64_t time_varying_objects = 0;
        for (const auto& diagnostic : source->conversion_diagnostics) {
            if (diagnostic.severity == scene::source::ConversionSeverity::Warning) ++warnings;
            if (diagnostic.severity == scene::source::ConversionSeverity::Error) ++errors;
            if (diagnostic.severity == scene::source::ConversionSeverity::Fatal) ++fatals;
            if (diagnostic.code.find("unsupported") != std::string::npos) ++unsupported;
        }
        for (const auto& mesh : source->polygon_meshes) time_varying_objects += mesh.time_varying ? 1 : 0;
        for (const auto& instancer : source->point_instancers) time_varying_objects += instancer.time_varying ? 1 : 0;
        for (const auto& camera : source->source_cameras) time_varying_objects += camera.time_varying ? 1 : 0;
        for (const auto& light : source->source_lights) time_varying_objects += light.time_varying ? 1 : 0;

        std::cout << "nodes=" << source->nodes.size() << '\n';
        std::cout << "polygon_meshes=" << source->polygon_meshes.size() << '\n';
        std::cout << "polygons=" << polygon_count << '\n';
        std::cout << "face_vertices=" << face_vertex_count << '\n';
        std::cout << "primvars=" << primvar_count << '\n';
        for (const auto& [name, count] : primvar_interpolations) {
            std::cout << "primvars_" << name << '=' << count << '\n';
        }
        std::cout << "native_prototypes=" << source->native_prototypes.size() << '\n';
        std::cout << "native_instances=" << source->native_instances.size() << '\n';
        std::cout << "point_instancers=" << source->point_instancers.size() << '\n';
        std::cout << "point_instancer_logical_instances=" << logical_instance_count << '\n';
        std::cout << "material_graphs=" << source->material_graphs.size() << '\n';
        std::cout << "shader_nodes=" << source->shader_nodes.size() << '\n';
        uint64_t shader_connections = 0;
        for (const auto& graph : source->material_graphs) shader_connections += graph.connections.size();
        std::cout << "shader_connections=" << shader_connections << '\n';
        std::cout << "assets=" << source->source_assets.size() << '\n';
        std::cout << "cameras=" << source->source_cameras.size() << '\n';
        std::cout << "lights=" << source->source_lights.size() << '\n';
        std::cout << "time_varying_objects=" << time_varying_objects << '\n';
        std::cout << "source_bounds_valid=" << source->metadata.source_bounds.is_valid << '\n';
        if (source->metadata.source_bounds.is_valid) {
            const auto& bounds = source->metadata.source_bounds;
            std::cout << "source_bounds_min=" << bounds.pos_min.x << ',' << bounds.pos_min.y << ',' << bounds.pos_min.z << '\n';
            std::cout << "source_bounds_max=" << bounds.pos_max.x << ',' << bounds.pos_max.y << ',' << bounds.pos_max.z << '\n';
        }
        unsupported += raw->statistics().unknown_schema_count;
        std::cout << "unsupported_schema_or_property=" << unsupported << '\n';
        std::cout << "warnings=" << warnings << '\n';
        std::cout << "errors=" << errors << '\n';
        std::cout << "fatals=" << fatals << '\n';
        return fatals == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "SceneRawJungle to SceneSource conversion failed: " << error.what() << '\n';
        return 1;
    }
}
