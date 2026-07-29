#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <string>

#include "util/Constants.h"

namespace util {

#define ProgramArgument_MAC \
    X(uint32_t, run_id, run-id) \
    X(std::string, run_name, run-name) \
    X(std::string, output_filepath, output-filepath) \
    X(uint32_t, renderer_variant, renderer-variant) \
    X(uint32_t, variable, variable) \
    X(bool, to_use_scene, to-use-scene) \
    X(bool, to_load_texture, to-load-texture) \
    X(bool, use_vfc, use-vfc) \
    X(uint32_t scene_variant, scene-variant) \
    X(std::string, scene_importer, scene-importer) \
    X(std::string, scene_path, scene-path) \
    X(uint32_t, warmup_frames, warmup-frames) \
    X(uint32_t, measure_frames, measure-frames) \
    X(bool, auto_terminate, auto-terminate) \
    X(bool, vsync, vsync) \
    X(uint32_t, camera_mode, camera-mode) \
    X(std::string, camera_filepath, camera-filepath) \
    X(uint32_t, camera_keyframe_interval, camera-keyframe-interval) \
    X(bool, to_set_start_frame, to-set-start-frame) \
    X(uint32_t, key_frame, key-frame) \
    X(uint32_t, profile_window_frames, profile-window-frames) \
    X(bool, capture_frames, capture-frames) \
    X(std::string, capture_output_dir, capture-output-dir) \
    X(uint32_t, capture_stride, capture-stride) \
    X(uint32_t, capture_fps, capture-fps) \
    X(uint32_t, window_width, window-width) \
    X(uint32_t, window_height, window-height) \
    X(uint32_t, seed, seed) \
    X(uint32_t, material_assign_strategy, material-assign-strategy) \
    X(uint32_t, material_assign_max_open, material-assign-max-open) \
    X(float, material_assign_locality, material-assign-locality) \
    X(float, material_assign_diversity, material-assign-diversity) \
    X(float, camera_pos_x, camera-pos-x) \
    X(float, camera_pos_y, camera-pos-y) \
    X(float, camera_pos_z, camera-pos-z) \
    X(float, camera_lookat_x, camera-lookat-x) \
    X(float, camera_lookat_y, camera-lookat-y) \
    X(float, camera_lookat_z, camera-lookat-z) \
    X(float, camera_near_z, camera-near-z) \
    X(float, camera_far_z, camera-far-z) \
    X(float, camera_fov, camera-fov) \
    X(uint32_t, object_count, object-count) \
    X(uint32_t, material_count, material-count) \
    X(uint32_t, geometry_count, geometry-count) \
    X(uint32_t, overdraw_count, overdraw-count) \
    X(bool, to_remain_only_in_camera, to-remain-only-in-camera) \
    X(float, z_min, z-min) \
    X(float, z_max, z-max) \
    X(float, xy_minmax, xy-minmax) \
    X(float, radius, radius) \
    X(uint32_t, geometry_div, geometry-div) \
    X(uint32_t, gbuffer_cnt, gbuffer-cnt) \
    X(uint32_t, texture_count, texture-count) \
    X(uint32_t, texture_size, texture-size) \
    X(uint32_t, texture_sampling_count, texture-sampling-count) \
    X(uint32_t, alu_calc_count, alu-calc-count)


#define ProgramResult_MAC \
    X(std::string, renderer_name, renderer_name) \
    X(std::string, run_current_time, run_current_time) \
    X(std::string, camera_mode_name, camera-mode-name) \
    X(double, total_time_min_ms, total_time_min_ms) \
    X(double, total_time_median_ms, total_time_median_ms) \
    X(double, total_time_max_ms, total_time_max_ms) \
    X(double, total_time_avg_ms, total_time_avg_ms) \
    X(double, total_time_p01_ms, total_time_p01_ms) \
    X(double, total_time_p10_ms, total_time_p10_ms) \
    X(double, total_time_p90_ms, total_time_p90_ms) \
    X(double, total_time_p99_ms, total_time_p99_ms) \
    X(std::uint32_t, variable_geometry_count, variable-geometry-count) \
    X(std::uint32_t, variable_overdraw_count, variable-overdraw-count) \
    X(std::uint32_t, variable_waste_quad_count, variable-waste-quad-count) \
    X(std::uint32_t, variable_alu_op_count, variable-alu-op-count)


    struct ProgramArgument {

#define X(type, name, arg) \
	type name;
        ProgramArgument_MAC
#undef X

    public:
        ProgramArgument();
        static ProgramArgument from_args(const std::vector<std::string>& args);
        static std::string get_header_string();
        std::string to_string() const;
    };

    struct ProgramResult {
        std::array<std::string, util::Constants::TIMER_SLOT_COUNT> pass_names{};
        std::array<double, util::Constants::TIMER_SLOT_COUNT> pass_time_avg_ms{};

#define X(type, name, arg) \
        type name{};
        ProgramResult_MAC
#undef X

    public:
        static ProgramResult from_args(const ProgramArgument& arg);
        static std::string get_header_string();
        std::string to_string() const;
    };
}
