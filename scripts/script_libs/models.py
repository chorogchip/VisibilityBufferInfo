from __future__ import annotations

from dataclasses import dataclass, fields


@dataclass
class ProgramArgument:
    """Arguments corresponding to the C++ util::ProgramArgument structure."""

    run_id: int = 0
    run_name: str = "no-run-name"
    output_filepath: str = "temp.csv"

    renderer_variant: int = 1
    variable: int = 0

    to_use_scene: bool = True
    to_load_texture: bool = False
    use_vfc: bool = True
    scene_path: str = (
        "assets/scenes/unpacked/main_sponza/"
        "NewSponza_Main_glTF_003.gltf"
    )

    warmup_frames: int = 60
    measure_frames: int = 24000
    auto_terminate: bool = False
    vsync: bool = True
    camera_mode: int = 1
    camera_filepath: str = "standard_camera.csv"
    camera_keyframe_interval: int = 10
    to_set_start_frame: bool = False
    key_frame: int = 0
    profile_window_frames: int = 10

    window_width: int = 1920
    window_height: int = 1080
    seed: int = 0

    camera_pos_x: float = 0.0
    camera_pos_y: float = 0.0
    camera_pos_z: float = -10.0
    camera_lookat_x: float = 0.0
    camera_lookat_y: float = 0.0
    camera_lookat_z: float = 0.0
    camera_near_z: float = 0.1
    camera_far_z: float = 1000.0
    camera_fov: float = 0.785

    object_count: int = 1
    material_count: int = 1
    geometry_count: int = 1
    overdraw_count: int = 0
    to_remain_only_in_camera: bool = False

    z_min: float = -1.0
    z_max: float = 1.0
    xy_minmax: float = 1.0
    radius: float = 0.5

    geometry_div: int = 1
    gbuffer_cnt: int = 1

    texture_count: int = 1
    texture_size: int = 256
    texture_sampling_count: int = 1
    alu_calc_count: int = 100


VALID_ARGUMENTS = {field.name for field in fields(ProgramArgument)}


PROGRAM_RESULT_PASS_COUNT = 32
PROGRAM_RESULT_FIELDS = (
    [
        field_name
        for pass_index in range(PROGRAM_RESULT_PASS_COUNT)
        for field_name in (
            f"pass_name_{pass_index}",
            f"pass_{pass_index}_time_avg_ms",
        )
    ]
    + [
        "renderer_name",
        "run_current_time",
        "camera-mode-name",
        "total_time_min_ms",
        "total_time_median_ms",
        "total_time_max_ms",
        "total_time_avg_ms",
        "total_time_p01_ms",
        "total_time_p10_ms",
        "total_time_p90_ms",
        "total_time_p99_ms",
        "variable-geometry-count",
        "variable-overdraw-count",
        "variable-waste-quad-count",
        "variable-alu-op-count",
    ]
)