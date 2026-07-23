#include "scene/SceneLoader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

#include "scene/SceneAssimpImporter.h"
#include "scene/SceneFastGltfImporter.h"
#include "scene/SceneBuilder.h"
#include "scene/SceneInfo.h"

namespace scene {

    namespace {

        bool is_gltf_path(const std::filesystem::path& path) {
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char value) {
                    return static_cast<char>(std::tolower(value));
                });
            return extension == ".gltf" || extension == ".glb";
        }

        bool use_fastgltf_importer(
            const std::string& importer,
            const std::filesystem::path& path) {

            return importer == "fastgltf" ||
                importer == "gltf" ||
                (importer == "auto" && is_gltf_path(path));
        }
    }

    std::unique_ptr<SceneDataCPU> SceneLoader::load(const util::ProgramArgument& arg) {
        if (arg.to_use_scene) {
            const std::filesystem::path scene_path =
                std::filesystem::current_path() / arg.scene_path;
            if (use_fastgltf_importer(arg.scene_importer, scene_path)) {
                return SceneFastGltfImporter::load(scene_path);
            }
            return SceneAssimpImporter::load(scene_path);
        }

        SceneInfoSphere gen_info{};
        gen_info.seed = arg.seed;
        gen_info.material_count = arg.material_count;
        gen_info.mesh_count = arg.geometry_count;
        gen_info.object_count = arg.object_count;
        gen_info.overdraw_count = arg.overdraw_count;
        gen_info.to_remain_only_in_camera = arg.to_remain_only_in_camera;
        gen_info.z_min = arg.z_min;
        gen_info.z_max = arg.z_max;
        gen_info.xy_minmax = arg.xy_minmax;
        gen_info.radius = arg.radius;
        gen_info.mesh_division = arg.geometry_div;
        gen_info.gbuffer_resource_count = arg.gbuffer_cnt;

        if (arg.variable == 1) return SceneBuilder::build_squares(gen_info);  // temp branch

        return SceneBuilder::build(gen_info);
    }

}
