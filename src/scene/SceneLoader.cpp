#include "scene/SceneLoader.h"

#include <filesystem>

#include "scene/SceneAssimpImporter.h"
#include "scene/SceneBuilder.h"
#include "scene/SceneInfo.h"

namespace scene {

    std::unique_ptr<SceneDataCPU> SceneLoader::load(const util::ProgramArgument& arg) {
        if (arg.to_use_scene) {
            return SceneAssimpImporter::load(std::filesystem::current_path() / arg.scene_path);
        }

        SceneInfoSphere gen_info{};
        gen_info.seed = arg.seed;
        gen_info.material_count = arg.material_count;
        gen_info.mesh_count = arg.geometry_count;
        gen_info.object_count = arg.object_count;
        gen_info.sort_from_front = arg.sort_type == 1;
        gen_info.sort_from_back = arg.sort_type == 2;
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
