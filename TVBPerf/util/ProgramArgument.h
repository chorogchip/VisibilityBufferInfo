#pragma once

#include <string>
#include <cstdint>

#define ProgramArgument_MAC \
    X(uint32_t, run_id, 0, run-id) \
    X(std::string, run_name, "no-run-name", run-name) \
    X(std::string, run_current_time, "", run-current-time) \
    X(std::string, output_filepath, "", output-filepath) \
    X(uint32_t, renderer_variant, 2, renderer-variant) \
    X(std::string, renderer_variant_name, "no-variant-name", renderer-variant-name) \
    X(bool, to_use_scene, false, to-use-scene) \
    X(std::string, scene_path, "assets/scenes/unpacked/CornellBox/CornellBox-Empty-RG.obj", scene-path) \
    X(uint32_t, warmup_frames, 60000, warmup-frames) \
    X(uint32_t, measure_frames, 12000, measure-frames) \
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
    X(uint32_t, sphere_count, 5, sphere-count) \
    X(uint32_t, material_count, 1, material-count) \
    X(uint32_t, geometry_count, 1, geometry-count) \
    X(float, z_min, -1.0f, z-min) \
    X(float, z_max, 1.0f, z-max) \
    X(float, xy_minmax, 1.0f, xy-minmax) \
    X(float, radius, 0.5f, radius) \
    X(uint32_t, geometry_div, 1, geometry-div) \
    X(uint32_t, gbuffer_cnt, 1, gbuffer-cnt) \
    X(uint32_t, texture_count, 0, texture-count) \
    X(uint32_t, texture_size, 1, texture-size) \
    X(uint32_t, texture_sampling_count, 0, texture-sampling-count) \
    X(uint32_t, alu_calc_count, 0, alu-calc-count)

struct ProgramArgument {
#define X(type, name, defl, arg) \
	type name = defl;
    ProgramArgument_MAC
#undef X
};