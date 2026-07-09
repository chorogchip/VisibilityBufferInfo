#pragma once

#include <string>
#include <cstdint>
#include <sstream>

#define ProgramArgument_MAC \
    X(uint32_t, run_id, 0, run-id) \
    X(std::string, run_name, "no-run-name", run-name) \
    X(std::string, run_current_time, "", run-current-time) \
    X(std::string, output_filepath, "temp.csv", output-filepath) \
    X(uint32_t, renderer_variant, 4, renderer-variant) \
    X(std::string, renderer_variant_name, "no-variant-name", renderer-variant-name) \
    X(uint32_t, variable, 0, variable) \
    X(bool, to_use_scene, true, to-use-scene) \
    X(std::string, scene_path, "assets/scenes/unpacked/CornellBox/CornellBox-Empty-RG.obj", scene-path) \
    X(uint32_t, warmup_frames, 600, warmup-frames) \
    X(uint32_t, measure_frames, 120, measure-frames) \
    X(bool, auto_terminate, false, auto-terminate) \
    X(uint32_t, window_width, 1280, window-width) \
    X(uint32_t, window_height, 720, window-height) \
    X(uint32_t, seed, 0, seed) \
    X(float, camera_pos_x, 5.0f, camera-pos-x) \
    X(float, camera_pos_y, 2.0f, camera-pos-y) \
    X(float, camera_pos_z, -10.0f, camera-pos-z) \
    X(float, camera_lookat_x, 0.0f, camera-lookat-x) \
    X(float, camera_lookat_y, 0.0f, camera-lookat-y) \
    X(float, camera_lookat_z, 0.0f, camera-lookat-z) \
    X(float, camera_near_z, 0.1f, camera-near-z) \
    X(float, camera_far_z, 1000.0f, camera-far-z) \
    X(float, camera_fov, 45.0f, camera-fov) \
    X(uint32_t, sphere_count, 50, sphere-count) \
    X(uint32_t, material_count, 1, material-count) \
    X(uint32_t, geometry_count, 1, geometry-count) \
    X(float, z_min, -1.0f, z-min) \
    X(float, z_max, 1.0f, z-max) \
    X(float, xy_minmax, 1.0f, xy-minmax) \
    X(float, radius, 0.5f, radius) \
    X(uint32_t, geometry_div, 10, geometry-div) \
    X(uint32_t, gbuffer_cnt, 1, gbuffer-cnt) \
    X(uint32_t, texture_count, 1, texture-count) \
    X(uint32_t, texture_size, 1, texture-size) \
    X(uint32_t, texture_sampling_count, 0, texture-sampling-count) \
    X(uint32_t, alu_calc_count, 0, alu-calc-count)

struct ProgramArgument {
#define X(type, name, defl, arg) \
	type name = defl;
    ProgramArgument_MAC
#undef X
};

inline std::string csv_escape(const std::string& value) {
    bool need_quote = false;
    for (char c : value) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            need_quote = true;
            break;
        }
    }

    if (!need_quote) return value;

    std::string ret = "\"";
    for (char c : value) {
        if (c == '"') ret += "\"\"";
        else ret += c;
    }
    ret += "\"";
    return ret;
}

inline std::string csv_value(const std::string& value) {
    return csv_escape(value);
}

inline std::string csv_value(bool value) {
    return value ? "1" : "0";
}

template <typename T>
inline std::string csv_value(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

inline std::string program_argument_csv_header() {
    std::string ret;
#define X(type, name, defl, arg) \
    ret += #name ",";
    ProgramArgument_MAC
#undef X
    if (!ret.empty()) ret.pop_back();
    return ret;
}

inline std::string program_argument_csv_values(const ProgramArgument& arg) {
    std::string ret;
#define X(type, name, defl, argname) \
    ret += csv_value(arg.name) + ",";
    ProgramArgument_MAC
#undef X
    if (!ret.empty()) ret.pop_back();
    return ret;
}
