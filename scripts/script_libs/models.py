from __future__ import annotations

from dataclasses import dataclass, fields


@dataclass
class ProgramArgument:
    """Arguments corresponding to the C++ util::ProgramArgument structure."""

    run_id: int = 0
    run_name: str = "no-run-name"
    run_current_time: str = ""
    output_filepath: str = "temp.csv"

    renderer_variant: int = 1
    renderer_variant_name: str = "no-variant-name"
    variable: int = 0

    to_use_scene: bool = False
    scene_path: str = (
        "assets/scenes/unpacked/main_sponza/"
        "NewSponza_Main_glTF_003.gltf"
    )

    warmup_frames: int = 600
    measure_frames: int = 120
    auto_terminate: bool = False

    window_width: int = 1280
    window_height: int = 720
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
    alu_calc_count: int = 1


VALID_ARGUMENTS = {field.name for field in fields(ProgramArgument)}
