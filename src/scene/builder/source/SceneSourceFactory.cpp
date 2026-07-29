#include "scene/builder/source/SceneSourceFactory.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <string>

#include "ProgramArgument.h"
#include "scene/builder/source/BuilderAssimp.h"
#include "scene/builder/source/BuilderQuads.h"
#include "scene/builder/source/BuilderMaterialGrid.h"
#include "scene/builder/source/VirtualMaterialClassBuilder.h"
#include "scene/builder/source/JungleSceneSourceBuilder.h"
#include "util/Logger.h"
#include "util/MyPath.h"

namespace scene {

    bool SceneSourceFactory::uses_jungle_builder(
        const util::ProgramArgument& argument) {

        if (!argument.to_use_scene) return false;
        if (argument.scene_importer == "jungle") return true;
        if (argument.scene_importer != "auto") return false;

        util::MyPath path{ argument.scene_path };
        return
            path.is_extention_lower_contain(".usd") ||
            path.is_extention_lower_contain(".usda");
    }

    std::unique_ptr<SceneSourceData> SceneSourceFactory::create_scene(const util::ProgramArgument& argument) {

        if (!argument.to_use_scene) {

            if (argument.scene_variant == 0) {

                BuilderQuads::BuilderQuadsConfig config{};
                config.object_count = argument.object_count;
                config.overdraw_count = argument.overdraw_count;
                config.division = argument.geometry_div;
                return BuilderQuads::build(config);

            } else if (argument.scene_variant == 1) {

                BuilderMaterialGrid::BuilderMaterialGridConfig config{};
                config.seed = argument.seed;
                config.triangle_division = argument.geometry_div;
                config.material_limit_open = argument.material_assign_max_open;
                config.material_locality = argument.material_assign_locality;
                config.material_diversity = argument.material_assign_diversity;
                return BuilderMaterialGrid::build(config);

            } else util::Logger::g_logger.assert_with_log(false, "wrong scene variant value");
        } 

        const auto path = util::MyPath(argument.scene_path).get_absolute();
        util::Logger::g_logger.assert_with_log(path.is_regular(), "scene source not exist");

        if (uses_jungle_builder(argument)) {
            return JungleSceneSourceBuilder::build(path.get());
        }

        auto ret = BuilderAssimp::build(path.get());

        VirtualMaterialClassBuilder::Params vmat_params{};
        vmat_params.seed = argument.seed;
        vmat_params.strategy = static_cast<VirtualMaterialClassBuilder::EnumAssignStrategy>(argument.material_assign_strategy);
        vmat_params.max_material_bin_limit = 255;
        vmat_params.max_material_real_open = argument.material_assign_max_open;
        vmat_params.material_diversity = argument.material_assign_diversity;
        VirtualMaterialClassBuilder::assign_materials(*ret, vmat_params);

        return ret;
    }
}
