#include "util/ProgramArgumentValidator.h"

#include <cmath>
#include <limits>

#include "util/Logger.h"

namespace util {

	void ProgramArgumentValidator::validate_program_args(const ProgramArgument& arg) {

        auto& logger = util::Logger::g_logger;

        logger.assert_with_log(arg.renderer_variant >= 1 && arg.renderer_variant <= 9,
            "renderer_variant must be between 1 and 9");
        logger.assert_with_log(arg.window_width > 0, "window_width must be greater than 0");
        logger.assert_with_log(arg.window_height > 0, "window_height must be greater than 0");
        logger.assert_with_log(arg.measure_frames > 0, "measure_frames must be greater than 0");
        logger.assert_with_log(arg.camera_mode <= 2,
            "camera_mode must be 0 (free), 1 (record), or 2 (playback)");
        logger.assert_with_log(!arg.camera_filepath.empty(), "camera_filepath must not be empty");
        logger.assert_with_log(arg.camera_keyframe_interval > 0,
            "camera_keyframe_interval must be greater than 0");
        if (arg.to_set_start_frame) {
            logger.assert_with_log(arg.camera_mode == 0,
                "camera_mode must be 0 when to_set_start_frame is true");
        }
        logger.assert_with_log(arg.profile_window_frames > 0,
            "profile_window_frames must be greater than 0");

        logger.assert_with_log(std::isfinite(arg.camera_pos_x), "camera_pos_x must be finite");
        logger.assert_with_log(std::isfinite(arg.camera_pos_y), "camera_pos_y must be finite");
        logger.assert_with_log(std::isfinite(arg.camera_pos_z), "camera_pos_z must be finite");
        logger.assert_with_log(std::isfinite(arg.camera_lookat_x), "camera_lookat_x must be finite");
        logger.assert_with_log(std::isfinite(arg.camera_lookat_y), "camera_lookat_y must be finite");
        logger.assert_with_log(std::isfinite(arg.camera_lookat_z), "camera_lookat_z must be finite");
        logger.assert_with_log(std::isfinite(arg.camera_near_z), "camera_near_z must be finite");
        logger.assert_with_log(std::isfinite(arg.camera_far_z), "camera_far_z must be finite");
        logger.assert_with_log(std::isfinite(arg.camera_fov), "camera_fov must be finite");
        logger.assert_with_log(arg.camera_near_z > 0.0f,
            "camera_near_z must be greater than 0");
        logger.assert_with_log(arg.camera_far_z > arg.camera_near_z,
            "camera_far_z must be greater than camera_near_z");
        logger.assert_with_log(arg.camera_fov > 0.0f && arg.camera_fov < 3.14159265358979323846f,
            "camera_fov must be between 0 and pi radians");

        logger.assert_with_log(arg.gbuffer_cnt >= 1 && arg.gbuffer_cnt <= 8,
            "gbuffer_cnt must be between 1 and 8");
        logger.assert_with_log(arg.texture_count > 0, "texture_count must be greater than 0");
        logger.assert_with_log(arg.texture_size > 0, "texture_size must be greater than 0");

        if (arg.to_use_scene) {
            logger.assert_with_log(!arg.scene_path.empty(),
                "scene_path must not be empty when to_use_scene is true");
            return;
        }

        logger.assert_with_log(arg.object_count > 0, "object_count must be greater than 0");
        logger.assert_with_log(arg.material_count > 0, "material_count must be greater than 0");
        logger.assert_with_log(arg.geometry_count > 0, "geometry_count must be greater than 0");
        logger.assert_with_log(arg.overdraw_count < arg.object_count,
            "overdraw_count must be less than object_count");
        logger.assert_with_log(arg.geometry_div > 0, "geometry_div must be greater than 0");

        logger.assert_with_log(std::isfinite(arg.z_min), "z_min must be finite");
        logger.assert_with_log(std::isfinite(arg.z_max), "z_max must be finite");
        logger.assert_with_log(std::isfinite(arg.xy_minmax), "xy_minmax must be finite");
        logger.assert_with_log(std::isfinite(arg.radius), "radius must be finite");
        logger.assert_with_log(arg.z_min < arg.z_max, "z_min must be less than z_max");
        logger.assert_with_log(arg.xy_minmax > 0.0f, "xy_minmax must be greater than 0");
        logger.assert_with_log(arg.radius > 0.0f, "radius must be greater than 0");

        const uint64_t division_plus_one = static_cast<uint64_t>(arg.geometry_div) + 1;
        logger.assert_with_log(division_plus_one <=
            std::numeric_limits<uint64_t>::max() / division_plus_one,
            "geometry_div is too large");
        const uint64_t vertices_per_object = division_plus_one * division_plus_one;
        logger.assert_with_log(static_cast<uint64_t>(arg.object_count) <=
            std::numeric_limits<uint32_t>::max() / vertices_per_object,
            "object_count and geometry_div produce too much geometry");
	}
}
