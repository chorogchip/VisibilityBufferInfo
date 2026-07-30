#!/usr/bin/env python3
"""Generate dense full-camera real-scene material-class sweeps."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parent

RENDERERS = (8, 9)

SCENES = (
    {
        "stem": "sponza",
        "scene_path": (
            "../../../../../assets/scenes/unpacked/main_sponza/"
            "NewSponza_Main_glTF_003.gltf"
        ),
        "camera_filepath": "../../../../../scripts/standard_camera.csv",
        "measure_frames": 2348,
        "camera_last_frame": 2347,
        "source_material_count": 29,
        "class_counts": (1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 29),
    },
    {
        "stem": "sponza_ivy",
        "scene_path": (
            "../../../../../assets/scenes/unpacked/main_sponza_ivy/"
            "NewSponza_Main_Ivy_glTF.gltf"
        ),
        "camera_filepath": "../../../../../scripts/standard_camera.csv",
        "measure_frames": 2348,
        "camera_last_frame": 2347,
        "source_material_count": 31,
        "class_counts": (1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 31),
    },
    {
        "stem": "bistro",
        "scene_path": (
            "../../../../../assets/scenes/unpacked/Bistro_v5_2/"
            "BistroExterior.fbx"
        ),
        "camera_filepath": "../../../../../scripts/standard_camera_bistro.csv",
        "measure_frames": 5474,
        "camera_last_frame": 5473,
        "source_material_count": 132,
        "class_counts": (
            1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20,
            24, 32, 40, 48, 64, 80, 96, 112, 128, 132,
        ),
    },
)


def make_base(scene: dict[str, object], textures: bool) -> dict[str, object]:
    return {
        "renderer_variant": 8,
        "visibility_debug_mode": 0,
        "donut_linear_gbuffer": False,
        "variable": 1,
        "to_use_scene": True,
        "to_load_texture": textures,
        "use_vfc": True,
        "scene_variant": 0,
        "scene_importer": "assimp",
        "scene_path": scene["scene_path"],
        "warmup_frames": 60,
        "measure_frames": scene["measure_frames"],
        "vsync": False,
        "camera_mode": 2,
        "camera_filepath": scene["camera_filepath"],
        "camera_keyframe_interval": 10,
        "to_set_start_frame": False,
        "key_frame": 0,
        "profile_window_frames": 50,
        "capture_frames": False,
        "capture_stride": 1,
        "capture_fps": 60,
        "window_width": 1920,
        "window_height": 1080,
        "seed": 0,
        "material_assign_strategy": 2,
        "material_assign_max_open": 1,
        "material_assign_locality": 1.0,
        "material_assign_diversity": 1.0,
        "geometry_div": 1,
        "camera_pos_x": 0.0,
        "camera_pos_y": 0.0,
        "camera_pos_z": -10.0,
        "camera_lookat_x": 0.0,
        "camera_lookat_y": 0.0,
        "camera_lookat_z": 0.0,
        "camera_near_z": 0.1,
        "camera_far_z": 1000.0,
        "camera_fov": 0.785,
        "object_count": 1,
        "material_count": 1,
        "geometry_count": 1,
        "overdraw_count": 0,
        "to_remain_only_in_camera": False,
        "z_min": -1.0,
        "z_max": 1.0,
        "xy_minmax": 1.0,
        "radius": 0.5,
        "gbuffer_cnt": 1,
        "texture_count": 1,
        "texture_size": 256,
        "texture_sampling_count": 1,
        "alu_calc_count": 100,
    }


def make_spec(scene: dict[str, object], textures: bool) -> dict[str, object]:
    samples = []
    for class_count in scene["class_counts"]:
        for renderer_variant in RENDERERS:
            samples.append(
                {
                    "renderer_variant": renderer_variant,
                    "variable": class_count,
                    "material_assign_max_open": class_count,
                }
            )

    texture_label = "on" if textures else "off"
    return {
        "_description": (
            f"Full-camera {scene['stem']} material-class scaling with "
            f"textures {texture_label}; balanced exact bins; Donut "
            "DeferredPrepass versus Donut VisGBuffer."
        ),
        "_status": "runnable_now",
        "_purpose": (
            "Measure how renderer total/pass GPU time changes as the number "
            "of occupied generic material classes grows, and separate "
            "texture sampling cost from class scheduling/reconstruction."
        ),
        "_hardware": "NVIDIA GeForce RTX 5060 Ti 16GB",
        "_source_material_count": scene["source_material_count"],
        "_camera_last_frame": scene["camera_last_frame"],
        "_class_counts": list(scene["class_counts"]),
        "_comparison_renderers": {
            "8": "DonutDeferredPrepass",
            "9": "DonutVisGBuffer",
        },
        "executable": "../../out/build/x64-Release/bin/Release/TVBPerf.exe",
        "repeat": 1,
        "timeout_seconds": 7200,
        "keep_individual_csv": True,
        "base": make_base(scene, textures),
        "samples": samples,
    }


def main() -> int:
    written = []
    for scene in SCENES:
        for textures in (False, True):
            texture_label = "texture_on" if textures else "texture_off"
            path = ROOT / (
                f"17_real_material_camera_{scene['stem']}_{texture_label}.json"
            )
            path.write_text(
                json.dumps(make_spec(scene, textures), indent=2) + "\n",
                encoding="utf-8",
            )
            written.append(path)

    for path in written:
        print(path.relative_to(ROOT.parent.parent))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
