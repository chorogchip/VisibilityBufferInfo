#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <string>

namespace util {

#define ProgramArgument_MAC \
    X(uint32_t, run_id, 0, run-id) \
    X(std::string, run_name, "no-run-name", run-name) \
    X(std::string, output_filepath, "temp.csv", output-filepath) \
    X(uint32_t, renderer_variant, 1, renderer-variant) \
    X(uint32_t, variable, 0, variable) \
    X(bool, to_use_scene, false, to-use-scene) \
    X(std::string, scene_path, "assets/scenes/unpacked/main_sponza/NewSponza_Main_glTF_003.gltf", scene-path) \
    X(uint32_t, warmup_frames, 600, warmup-frames) \
    X(uint32_t, measure_frames, 120, measure-frames) \
    X(bool, auto_terminate, false, auto-terminate) \
    X(bool, vsync, false, vsync) \
    X(uint32_t, camera_mode, 0, camera-mode) \
    X(std::string, camera_filepath, "camera.csv", camera-filepath) \
    X(uint32_t, camera_keyframe_interval, 10, camera-keyframe-interval) \
    X(uint32_t, profile_window_frames, 10, profile-window-frames) \
    X(uint32_t, window_width, 1280, window-width) \
    X(uint32_t, window_height, 720, window-height) \
    X(uint32_t, seed, 0, seed) \
    X(float, camera_pos_x, 0.0f, camera-pos-x) \
    X(float, camera_pos_y, 0.0f, camera-pos-y) \
    X(float, camera_pos_z, -10.0f, camera-pos-z) \
    X(float, camera_lookat_x, 0.0f, camera-lookat-x) \
    X(float, camera_lookat_y, 0.0f, camera-lookat-y) \
    X(float, camera_lookat_z, 0.0f, camera-lookat-z) \
    X(float, camera_near_z, 0.1f, camera-near-z) \
    X(float, camera_far_z, 1000.0f, camera-far-z) \
    X(float, camera_fov, 0.785f, camera-fov) \
    X(uint32_t, object_count, 1, object-count) \
    X(uint32_t, material_count, 1, material-count) \
    X(uint32_t, geometry_count, 1, geometry-count) \
    X(uint32_t, overdraw_count, 0, overdraw-count) \
    X(bool, to_remain_only_in_camera, false, to-remain-only-in-camera) \
    X(float, z_min, -1.0f, z-min) \
    X(float, z_max, 1.0f, z-max) \
    X(float, xy_minmax, 1.0f, xy-minmax) \
    X(float, radius, 0.5f, radius) \
    X(uint32_t, geometry_div, 1, geometry-div) \
    X(uint32_t, gbuffer_cnt, 1, gbuffer-cnt) \
    X(uint32_t, texture_count, 1, texture-count) \
    X(uint32_t, texture_size, 256, texture-size) \
    X(uint32_t, texture_sampling_count, 1, texture-sampling-count) \
    X(uint32_t, alu_calc_count, 1, alu-calc-count)


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

#define ProgramResultPerFrame_MAC


    struct ProgramArgument {
        static constexpr uint32_t CAMERA_MODE_FREE = 0;
        static constexpr uint32_t CAMERA_MODE_RECORD = 1;
        static constexpr uint32_t CAMERA_MODE_PLAYBACK = 2;

#define X(type, name, defl, arg) \
	type name = defl;
        ProgramArgument_MAC
#undef X
    public:
        static ProgramArgument from_args(const std::vector<std::string>& args);
        static std::string camera_mode_to_string(uint32_t mode);
        void validate() const;

        static std::string get_header_string();
        std::string to_string() const;
    };

    struct ProgramResult {
        static constexpr std::size_t PASS_COUNT = 32;

        std::array<std::string, PASS_COUNT> pass_names{};
        std::array<double, PASS_COUNT> pass_time_avg_ms{};

#define X(type, name, arg) \
        type name{};
        ProgramResult_MAC
#undef X
    public:
        static std::string get_header_string();
        std::string to_string() const;
    };

    struct ProgramResultPerFrame {
#define X(type, name, arg) \
    type name;
        ProgramResultPerFrame_MAC
#undef X
    public:
        static std::string get_header_string();
        std::string to_string() const;
    };
}
