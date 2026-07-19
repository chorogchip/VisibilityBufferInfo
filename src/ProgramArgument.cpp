#include "ProgramArgument.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

#include "util/Logger.h"
#include "util/StringUtils.h"

namespace util {


    ProgramArgument ProgramArgument::from_args(const std::vector<std::string>& args) {
        util::ProgramArgument ret{};

        for (size_t i = 0; i < args.size(); ++i) {

#define X(type, name, defl, arg) \
        if (args[i] == std::string("--" #arg)) { \
            if (i + 1 >= args.size()) { \
                Logger::g_logger.assert_with_log(false, "Missing value for --" #arg); \
            } \
            ret.name = util::StringUtils::parse_value<type>(args[i + 1]); \
            ++i; \
            continue; \
        }
            ProgramArgument_MAC
#undef X
        };

        return ret;
    }

    std::string ProgramArgument::camera_mode_to_string(uint32_t mode) {
        switch (mode) {
        case CAMERA_MODE_FREE: return "free";
        case CAMERA_MODE_RECORD: return "record";
        case CAMERA_MODE_PLAYBACK: return "playback";
        default: return "unknown";
        }
    }

    void ProgramArgument::validate() const {
        auto& logger = util::Logger::g_logger;

        logger.assert_with_log(renderer_variant >= 1 && renderer_variant <= 6,
            "renderer_variant must be between 1 and 6");
        logger.assert_with_log(window_width > 0, "window_width must be greater than 0");
        logger.assert_with_log(window_height > 0, "window_height must be greater than 0");
        logger.assert_with_log(measure_frames > 0, "measure_frames must be greater than 0");
        logger.assert_with_log(camera_mode <= CAMERA_MODE_PLAYBACK,
            "camera_mode must be 0 (free), 1 (record), or 2 (playback)");
        logger.assert_with_log(!camera_filepath.empty(), "camera_filepath must not be empty");
        logger.assert_with_log(camera_keyframe_interval > 0,
            "camera_keyframe_interval must be greater than 0");
        logger.assert_with_log(profile_window_frames > 0,
            "profile_window_frames must be greater than 0");

        logger.assert_with_log(std::isfinite(camera_pos_x), "camera_pos_x must be finite");
        logger.assert_with_log(std::isfinite(camera_pos_y), "camera_pos_y must be finite");
        logger.assert_with_log(std::isfinite(camera_pos_z), "camera_pos_z must be finite");
        logger.assert_with_log(std::isfinite(camera_lookat_x), "camera_lookat_x must be finite");
        logger.assert_with_log(std::isfinite(camera_lookat_y), "camera_lookat_y must be finite");
        logger.assert_with_log(std::isfinite(camera_lookat_z), "camera_lookat_z must be finite");
        logger.assert_with_log(std::isfinite(camera_near_z), "camera_near_z must be finite");
        logger.assert_with_log(std::isfinite(camera_far_z), "camera_far_z must be finite");
        logger.assert_with_log(std::isfinite(camera_fov), "camera_fov must be finite");
        logger.assert_with_log(camera_near_z > 0.0f,
            "camera_near_z must be greater than 0");
        logger.assert_with_log(camera_far_z > camera_near_z,
            "camera_far_z must be greater than camera_near_z");
        logger.assert_with_log(camera_fov > 0.0f && camera_fov < 3.14159265358979323846f,
            "camera_fov must be between 0 and pi radians");

        logger.assert_with_log(gbuffer_cnt >= 1 && gbuffer_cnt <= 8,
            "gbuffer_cnt must be between 1 and 8");
        logger.assert_with_log(texture_count > 0, "texture_count must be greater than 0");
        logger.assert_with_log(texture_size > 0, "texture_size must be greater than 0");

        if (to_use_scene) {
            logger.assert_with_log(!scene_path.empty(),
                "scene_path must not be empty when to_use_scene is true");
            return;
        }

        logger.assert_with_log(object_count > 0, "object_count must be greater than 0");
        logger.assert_with_log(material_count > 0, "material_count must be greater than 0");
        logger.assert_with_log(geometry_count > 0, "geometry_count must be greater than 0");
        logger.assert_with_log(overdraw_count < object_count,
            "overdraw_count must be less than object_count");
        logger.assert_with_log(geometry_div > 0, "geometry_div must be greater than 0");

        logger.assert_with_log(std::isfinite(z_min), "z_min must be finite");
        logger.assert_with_log(std::isfinite(z_max), "z_max must be finite");
        logger.assert_with_log(std::isfinite(xy_minmax), "xy_minmax must be finite");
        logger.assert_with_log(std::isfinite(radius), "radius must be finite");
        logger.assert_with_log(z_min < z_max, "z_min must be less than z_max");
        logger.assert_with_log(xy_minmax > 0.0f, "xy_minmax must be greater than 0");
        logger.assert_with_log(radius > 0.0f, "radius must be greater than 0");

        const uint64_t division_plus_one = static_cast<uint64_t>(geometry_div) + 1;
        logger.assert_with_log(division_plus_one <=
            std::numeric_limits<uint64_t>::max() / division_plus_one,
            "geometry_div is too large");
        const uint64_t vertices_per_object = division_plus_one * division_plus_one;
        logger.assert_with_log(static_cast<uint64_t>(object_count) <=
            std::numeric_limits<uint32_t>::max() / vertices_per_object,
            "object_count and geometry_div produce too much geometry");
    }

    std::string ProgramArgument::get_header_string() {
        std::ostringstream stream;
#define X(type, name, defl, argname) \
        stream << (#argname) << ',';
        ProgramArgument_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }

    std::string ProgramArgument::to_string() const {
        std::ostringstream stream;
#define X(type, name, defl, argname) \
        stream << (this->name) << ',';
        ProgramArgument_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }


    std::string ProgramResult::get_header_string() {
        std::ostringstream stream;

        for (size_t i = 0; i < PASS_COUNT; ++i) {
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

        for (size_t i = 0; i < PASS_COUNT; ++i) {
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


    std::string ProgramResultPerFrame::get_header_string() {
        std::ostringstream stream;
#define X(type, name, argname) \
        stream << (#argname) << ',';
        ProgramResultPerFrame_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }

    std::string ProgramResultPerFrame::to_string() const {
        std::ostringstream stream;
#define X(type, name, argname) \
        stream << (this->name) << ',';
        ProgramResultPerFrame_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }
}
