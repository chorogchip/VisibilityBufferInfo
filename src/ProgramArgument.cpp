#include "ProgramArgument.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "util/Constants.h"
#include "util/Logger.h"
#include "util/StringUtils.h"
#include "util/TimeUtils.h"

namespace util {

    ProgramArgument::ProgramArgument()
        : run_id(0),
        run_name("no-run-name"),
        output_filepath("out_result.csv"),
        renderer_variant(9),
        variable(0),
        to_use_scene(true),
        to_load_texture(true),
        use_vfc(true),
        scene_variant(0),
        scene_importer("auto"),
        scene_path(
            1 ? "assets/scenes/unpacked/Bistro_v5_2/BistroExterior.fbx" :
            1 ? "assets/scenes/unpacked/main_sponza/NewSponza_Main_glTF_003.gltf" :
            1 ? "assets/scenes/unpacked/Bistro_v5_2/BistroInterior_Wine.fbx" :
            1 ? "assets/scenes/unpacked/breakfast_room/breakfast_room.obj" :
            1 ? "assets/scenes/unpacked/San_Miguel/san-miguel.obj" :
            1 ? "assets/scenes/unpacked/SunTemple_v4/SunTemple.fbx" :
            1 ? "assets/scenes/unpacked/ZeroDay_v1/MEASURE_ONE/MEASURE_ONE.fbx" :
            ""),
        warmup_frames(60),
        measure_frames(5500),
        auto_terminate(false),
        vsync(true),
        camera_mode(0),
        camera_filepath("standard_camera_bistro.csv"),
        camera_keyframe_interval(10),
        to_set_start_frame(false),
        key_frame(0),
        profile_window_frames(10),
        capture_frames(false),
        capture_output_dir("captures"),
        capture_stride(1),
        capture_fps(60),
        window_width(1280),
        window_height(720),
        seed(0),
        material_assign_strategy(0),
        material_assign_max_open(255),
        material_assign_locality(1.0f),
        material_assign_diversity(0.0f),
        camera_pos_x(0.0f),
        camera_pos_y(0.0f),
        camera_pos_z(-10.0f),
        camera_lookat_x(0.0f),
        camera_lookat_y(0.0f),
        camera_lookat_z(0.0f),
        camera_near_z(0.1f),
        camera_far_z(1000.0f),
        camera_fov(0.785f),
        object_count(1),
        material_count(1),
        geometry_count(1),
        overdraw_count(0),
        to_remain_only_in_camera(false),
        z_min(-1.0f),
        z_max(1.0f),
        xy_minmax(1.0f),
        radius(0.5f),
        geometry_div(1),
        gbuffer_cnt(1),
        texture_count(1),
        texture_size(256),
        texture_sampling_count(1),
        alu_calc_count(100) {}

namespace {

    std::string normalize_argument_name(const std::string& argument) {
        std::string normalized = argument;
        if (normalized.rfind("--", 0) == 0) {
            std::replace(normalized.begin() + 2, normalized.end(), '_', '-');
        }
        return normalized;
    }

}

    ProgramArgument ProgramArgument::from_args(const std::vector<std::string>& args) {
        util::ProgramArgument ret{};

        for (size_t i = 0; i < args.size();) {
            const std::string option_name = normalize_argument_name(args[i]);

#define X(type, name, arg) \
        if (option_name == std::string("--" #arg)) { \
            if (i + 1 >= args.size()) { \
                Logger::g_logger.assert_with_log(false, "Missing value for --" #arg); \
            } \
            ret.name = util::StringUtils::parse_value<type>(args[i + 1]); \
            i += 2; \
            continue; \
        }
            ProgramArgument_MAC
#undef X

            Logger::g_logger << "unknown ProgramArgument: " << args[i] << '\n';
            Logger::g_logger.assert_with_log(false, "Unknown ProgramArgument");
            ++i;
        };

        return ret;
    }

    std::string ProgramArgument::get_header_string() {
        std::ostringstream stream;
#define X(type, name, argname) \
        stream << (#argname) << ',';
        ProgramArgument_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }

    std::string ProgramArgument::to_string() const {
        std::ostringstream stream;
#define X(type, name, argname) \
        stream << (this->name) << ',';
        ProgramArgument_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }

    ProgramResult ProgramResult::from_args(const ProgramArgument& arg) {

        ProgramResult ret{};

        ret.camera_mode_name =
            arg.camera_mode == 0 ? "free" :
            arg.camera_mode == 1 ? "record" :
            arg.camera_mode == 2 ? "playback" :
            "unknown";
        ret.run_current_time = util::TimeUtils::make_current_time_string();

        return ret;
    }

    std::string ProgramResult::get_header_string() {
        std::ostringstream stream;

        for (size_t i = 0; i < util::Constants::TIMER_SLOT_COUNT; ++i) {
            stream << "pass_name_" << i << ',';
            stream << "pass_" << i << "_time_avg_ms,";
        }

#define X(type, name, argname) \
        stream << (#argname) << ',';
        ProgramResult_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }

    std::string ProgramResult::to_string() const {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(5);

        for (size_t i = 0; i < util::Constants::TIMER_SLOT_COUNT; ++i) {
            stream << pass_names[i] << ',';
            stream << pass_time_avg_ms[i] << ',';
        }

#define X(type, name, argname) \
        stream << (this->name) << ',';
        ProgramResult_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }
}
