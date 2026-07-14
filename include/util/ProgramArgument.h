#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace util {

#define ProgramArgument_MAC \
    X(uint32_t, run_id, 0, run-id) \
    X(std::string, run_name, "no-run-name", run-name) \
    X(std::string, run_current_time, "", run-current-time) \
    X(std::string, output_filepath, "temp.csv", output-filepath) \
    X(uint32_t, renderer_variant, 1, renderer-variant) \
    X(std::string, renderer_variant_name, "no-variant-name", renderer-variant-name) \
    X(uint32_t, variable, 0, variable) \
    X(bool, to_use_scene, false, to-use-scene) \
    X(std::string, scene_path, "assets/scenes/unpacked/main_sponza/NewSponza_Main_glTF_003.gltf", scene-path) \
    X(uint32_t, warmup_frames, 600, warmup-frames) \
    X(uint32_t, measure_frames, 120, measure-frames) \
    X(bool, auto_terminate, false, auto-terminate) \
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
    X(bool, sort_from_front, false, sort-from-front) \
    X(bool, sort_from_back, false, sort-from-back) \
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


#define ProgramResultPass_MAC(X, index) \
    X(double, pass_##index##_time_min_ms, pass_##index##_time_min_ms) \
    X(double, pass_##index##_time_median_ms, pass_##index##_time_median_ms) \
    X(double, pass_##index##_time_max_ms, pass_##index##_time_max_ms) \
    X(double, pass_##index##_time_avg_ms, pass_##index##_time_avg_ms) \
    X(double, pass_##index##_time_p01_ms, pass_##index##_time_p01_ms) \
    X(double, pass_##index##_time_p10_ms, pass_##index##_time_p10_ms) \
    X(double, pass_##index##_time_p90_ms, pass_##index##_time_p90_ms) \
    X(double, pass_##index##_time_p99_ms, pass_##index##_time_p99_ms)

#define ProgramResult_MAC \
    X(std::string, renderer_name, renderer_name) \
    X(std::string, pass_name_0, pass_name_0) \
    X(std::string, pass_name_1, pass_name_1) \
    X(std::string, pass_name_2, pass_name_2) \
    X(std::string, pass_name_3, pass_name_3) \
    ProgramResultPass_MAC(X, 0) \
    ProgramResultPass_MAC(X, 1) \
    ProgramResultPass_MAC(X, 2) \
    ProgramResultPass_MAC(X, 3)


#define ProgramResultPerFrame_MAC


    struct ProgramArgument {
#define X(type, name, defl, arg) \
	type name = defl;
        ProgramArgument_MAC
#undef X
    public:
        static ProgramArgument from_args(const std::vector<std::string>& args);

        static std::string get_header_string();
        std::string to_string() const;
    };

    struct ProgramResult {
#define X(type, name, arg) \
    type name;
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
