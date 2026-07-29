"""Generate the post-fairness experiment specs.

The generated JSON files are intentionally independent campaigns. They are run
one at a time with scripts/run.py; this file never launches TVBPerf.
"""

from __future__ import annotations

import itertools
import json
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parent
HARDWARE = "NVIDIA GeForce RTX 5060 Ti 16GB"
ARCHIVE_HARDWARE = "Earlier archived results under datas/: NVIDIA GeForce RTX 5070"


def synthetic_base() -> dict[str, Any]:
    return {
        "renderer_variant": 8,
        "visibility_debug_mode": 2,
        "donut_linear_gbuffer": False,
        "variable": 0,
        "to_use_scene": False,
        "to_load_texture": False,
        "use_vfc": False,
        "scene_variant": 1,
        "scene_importer": "auto",
        "scene_path": "unused",
        "warmup_frames": 120,
        "measure_frames": 600,
        "vsync": False,
        "camera_mode": 0,
        "camera_filepath": "../../../../../scripts/standard_camera.csv",
        "camera_keyframe_interval": 10,
        "to_set_start_frame": False,
        "key_frame": 0,
        "profile_window_frames": 60,
        "capture_frames": False,
        "capture_stride": 1,
        "capture_fps": 60,
        "window_width": 1920,
        "window_height": 1080,
        "seed": 0,
        "material_assign_strategy": 0,
        "material_assign_max_open": 64,
        "material_assign_locality": 1.0,
        "material_assign_diversity": 1.0,
        "geometry_div": 128,
        "camera_pos_x": 0.0,
        "camera_pos_y": 0.0,
        "camera_pos_z": -3.0,
        "camera_lookat_x": 0.0,
        "camera_lookat_y": 0.0,
        "camera_lookat_z": 0.0,
        "camera_near_z": 0.1,
        "camera_far_z": 1000.0,
        "camera_fov": 0.785,
        "object_count": 1,
        "material_count": 255,
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


def real_base() -> dict[str, Any]:
    base = synthetic_base()
    base.update(
        {
            "to_use_scene": True,
            "to_load_texture": True,
            "use_vfc": True,
            "scene_variant": 0,
            "scene_importer": "assimp",
            "scene_path": "../../../../../assets/scenes/unpacked/main_sponza/NewSponza_Main_glTF_003.gltf",
            "warmup_frames": 60,
            "measure_frames": 600,
            "camera_mode": 2,
            "camera_filepath": "../../../../../scripts/standard_camera.csv",
            "geometry_div": 1,
            "material_count": 1,
        }
    )
    return base


SCENES = (
    {
        "scene_label": "Sponza",
        "scene_path": "../../../../../assets/scenes/unpacked/main_sponza/NewSponza_Main_glTF_003.gltf",
        "camera_filepath": "../../../../../scripts/standard_camera.csv",
        "full_measure_frames": 2500,
    },
    {
        "scene_label": "SponzaIvy",
        "scene_path": "../../../../../assets/scenes/unpacked/main_sponza_ivy/NewSponza_Main_Ivy_glTF.gltf",
        "camera_filepath": "../../../../../scripts/standard_camera.csv",
        "full_measure_frames": 2500,
    },
    {
        "scene_label": "Bistro",
        "scene_path": "../../../../../assets/scenes/unpacked/Bistro_v5_2/BistroExterior.fbx",
        "camera_filepath": "../../../../../scripts/standard_camera_bistro.csv",
        "full_measure_frames": 5500,
    },
)


def paired_samples(
    conditions: Iterable[dict[str, Any]],
    *,
    seeds: Iterable[int] = (0,),
    linear_values: Iterable[bool] = (False,),
) -> list[dict[str, Any]]:
    samples: list[dict[str, Any]] = []
    for condition, seed, linear, renderer in itertools.product(
        conditions, seeds, linear_values, (8, 9)
    ):
        sample = dict(condition)
        sample.update(
            {
                "variable": len(samples),
                "seed": seed,
                "donut_linear_gbuffer": linear,
                "renderer_variant": renderer,
            }
        )
        samples.append(sample)
    return samples


def spec(
    description: str,
    base: dict[str, Any],
    samples: list[dict[str, Any]],
    *,
    timeout_seconds: int = 3600,
    keep_individual_csv: bool = False,
) -> dict[str, Any]:
    return {
        "_description": description,
        "_status": "runnable_now",
        "_hardware": HARDWARE,
        "_archive_hardware_note": ARCHIVE_HARDWARE,
        "executable": "../../out/build/x64-Release/bin/Release/TVBPerf.exe",
        "repeat": 1,
        "timeout_seconds": timeout_seconds,
        "keep_individual_csv": keep_individual_csv,
        "base": base,
        "samples": samples,
    }


def write(name: str, value: dict[str, Any]) -> None:
    (ROOT / name).write_text(
        json.dumps(value, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


def main() -> None:
    counts = (1, 2, 4, 8, 16, 24, 32, 48, 64, 96, 128, 192, 255)
    dense_unit = (
        0.0,
        0.05,
        0.1,
        0.2,
        0.3,
        0.4,
        0.5,
        0.6,
        0.7,
        0.8,
        0.9,
        0.95,
        1.0,
    )
    phase = (0.0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0)

    base = synthetic_base()
    base.update({"warmup_frames": 60, "measure_frames": 180})
    write(
        "01_synth_decoupling_smoke.json",
        spec(
            "Validate decoupled material and class counts with both performance renderers.",
            base,
            paired_samples(
                (
                    {"material_count": 1, "material_assign_max_open": 1},
                    {"material_count": 64, "material_assign_max_open": 1},
                    {"material_count": 64, "material_assign_max_open": 8},
                )
            ),
            keep_individual_csv=True,
        ),
    )

    write(
        "02_synth_material_count_same_class_dense.json",
        spec(
            "Isolate material/draw count while every material maps to one generic PSO class.",
            synthetic_base(),
            paired_samples(
                (
                    {
                        "material_count": count,
                        "material_assign_max_open": 1,
                        "material_assign_locality": 1.0,
                        "material_assign_diversity": 1.0,
                    }
                    for count in counts
                ),
                seeds=(0, 1, 2),
            ),
        ),
    )

    write(
        "03_synth_class_count_fixed_materials_dense.json",
        spec(
            "Isolate generic PSO/material-bin count with 255 materials and fixed geometry.",
            synthetic_base(),
            paired_samples(
                (
                    {
                        "material_count": 255,
                        "material_assign_max_open": count,
                        "material_assign_locality": 0.5,
                        "material_assign_diversity": 1.0,
                    }
                    for count in counts
                ),
                seeds=(0, 1, 2),
            ),
        ),
    )

    write(
        "04_synth_locality_dense.json",
        spec(
            "Resolve the locality response curve at fixed material and class counts.",
            synthetic_base(),
            paired_samples(
                (
                    {"material_assign_locality": value}
                    for value in dense_unit
                ),
                seeds=(0, 1, 2),
            ),
        ),
    )

    write(
        "05_synth_diversity_dense.json",
        spec(
            "Resolve material-frequency diversity at both locality extremes.",
            synthetic_base(),
            paired_samples(
                (
                    {
                        "material_assign_diversity": diversity,
                        "material_assign_locality": locality,
                    }
                    for locality, diversity in itertools.product(
                        (0.0, 1.0), dense_unit
                    )
                ),
                seeds=(0, 1, 2),
            ),
        ),
    )

    phase_base = synthetic_base()
    phase_base["measure_frames"] = 360
    write(
        "06_synth_locality_diversity_phase_dense.json",
        spec(
            "Dense locality-by-diversity phase map for renderer crossover analysis.",
            phase_base,
            paired_samples(
                (
                    {
                        "material_assign_locality": locality,
                        "material_assign_diversity": diversity,
                    }
                    for locality, diversity in itertools.product(phase, phase)
                ),
                seeds=(0, 1),
            ),
            timeout_seconds=5400,
        ),
    )

    matrix_conditions = []
    for material_count in (16, 32, 64, 128, 255):
        for class_count in counts:
            if class_count <= material_count:
                matrix_conditions.append(
                    {
                        "material_count": material_count,
                        "material_assign_max_open": class_count,
                        "material_assign_locality": 0.5,
                    }
                )
    matrix_base = synthetic_base()
    matrix_base["measure_frames"] = 480
    write(
        "07_synth_material_class_matrix.json",
        spec(
            "Separate material/draw count from generic PSO class count over a two-dimensional matrix.",
            matrix_base,
            paired_samples(matrix_conditions, seeds=(0, 1)),
            timeout_seconds=5400,
        ),
    )

    workload_base = synthetic_base()
    workload_base["measure_frames"] = 480
    write(
        "08_synth_workload_scaling_dense.json",
        spec(
            "Measure geometry workload scaling at low, medium, and high locality.",
            workload_base,
            paired_samples(
                (
                    {
                        "geometry_div": geometry,
                        "material_assign_locality": locality,
                    }
                    for geometry, locality in itertools.product(
                        (16, 24, 32, 48, 64, 96, 128, 192, 256),
                        (0.0, 0.5, 1.0),
                    )
                ),
                seeds=(0, 1),
            ),
        ),
    )

    resolution_base = synthetic_base()
    resolution_base["measure_frames"] = 480
    resolutions = (
        (960, 540),
        (1280, 720),
        (1600, 900),
        (1920, 1080),
        (2560, 1440),
        (3200, 1800),
        (3840, 2160),
    )
    write(
        "09_synth_resolution_scaling_dense.json",
        spec(
            "Measure resolution scaling at three locality levels.",
            resolution_base,
            paired_samples(
                (
                    {
                        "window_width": width,
                        "window_height": height,
                        "material_assign_locality": locality,
                    }
                    for (width, height), locality in itertools.product(
                        resolutions, (0.0, 0.5, 1.0)
                    )
                )
            ),
        ),
    )

    write(
        "10_synth_seed_robustness_dense.json",
        spec(
            "Quantify seed variance at the midpoint locality/diversity condition.",
            synthetic_base(),
            paired_samples(
                (
                    {
                        "material_assign_locality": 0.5,
                        "material_assign_diversity": 0.5,
                    },
                ),
                seeds=range(20),
            ),
        ),
    )

    control_conditions = (
        {
            "material_count": material_count,
            "material_assign_max_open": class_count,
            "material_assign_locality": locality,
        }
        for (material_count, class_count), locality in itertools.product(
            ((1, 1), (64, 1), (255, 64)),
            (0.0, 1.0),
        )
    )
    write(
        "11_synth_linear_gbuffer_control.json",
        spec(
            "Separate end-to-end sRGB/UAV cost from scheduling and reconstruction cost.",
            synthetic_base(),
            paired_samples(
                control_conditions,
                seeds=(0, 1),
                linear_values=(False, True),
            ),
        ),
    )

    real_classes = (1, 2, 4, 8, 16, 32, 64)
    real_class_conditions = []
    for scene, class_count in itertools.product(SCENES, real_classes):
        real_class_conditions.append(
            {
                "scene_path": scene["scene_path"],
                "camera_filepath": scene["camera_filepath"],
                "material_assign_strategy": 0,
                "material_assign_max_open": class_count,
                "material_assign_diversity": 1.0,
            }
        )
    write(
        "12_real_class_count_dense.json",
        spec(
            "Measure generic PSO class-count scaling on Sponza, Sponza Ivy, and Bistro.",
            real_base(),
            paired_samples(real_class_conditions),
            timeout_seconds=5400,
            keep_individual_csv=True,
        ),
    )

    real_diversity_conditions = []
    for scene, diversity in itertools.product(SCENES, dense_unit):
        real_diversity_conditions.append(
            {
                "scene_path": scene["scene_path"],
                "camera_filepath": scene["camera_filepath"],
                "material_assign_strategy": 0,
                "material_assign_max_open": 64,
                "material_assign_diversity": diversity,
            }
        )
    write(
        "13_real_diversity_dense.json",
        spec(
            "Resolve real-scene material-to-class frequency diversity.",
            real_base(),
            paired_samples(real_diversity_conditions),
            timeout_seconds=5400,
            keep_individual_csv=True,
        ),
    )

    full_controls = []
    for scene in SCENES:
        full_controls.append(
            {
                "scene_path": scene["scene_path"],
                "camera_filepath": scene["camera_filepath"],
                "measure_frames": scene["full_measure_frames"],
                "material_assign_strategy": 1,
                "material_assign_max_open": 255,
            }
        )
    write(
        "14_real_full_camera_linear_control.json",
        spec(
            "Full camera comparison of the end-to-end sRGB ABI and linear G-buffer control.",
            real_base(),
            paired_samples(
                full_controls,
                linear_values=(False, True),
            ),
            timeout_seconds=7200,
            keep_individual_csv=True,
        ),
    )

    ablations = []
    for scene, load_texture, use_vfc in itertools.product(
        SCENES, (False, True), (False, True)
    ):
        ablations.append(
            {
                "scene_path": scene["scene_path"],
                "camera_filepath": scene["camera_filepath"],
                "to_load_texture": load_texture,
                "use_vfc": use_vfc,
                "material_assign_strategy": 1,
                "material_assign_max_open": 255,
            }
        )
    write(
        "15_real_texture_vfc_ablation.json",
        spec(
            "Separate texture loading and CPU view-frustum culling effects on both performance renderers.",
            real_base(),
            paired_samples(ablations),
            timeout_seconds=5400,
            keep_individual_csv=True,
        ),
    )

    raster_samples = []
    for scene in SCENES:
        raster_samples.append(
            {
                "variable": len(raster_samples),
                "renderer_variant": 10,
                "scene_path": scene["scene_path"],
                "camera_filepath": scene["camera_filepath"],
                "measure_frames": scene["full_measure_frames"],
                "to_load_texture": False,
                "material_assign_strategy": 1,
                "material_assign_max_open": 255,
            }
        )
    write(
        "16_real_software_raster_reference.json",
        spec(
            "Full-camera software-raster workload proxies aligned to the three real scenes.",
            real_base(),
            raster_samples,
            timeout_seconds=7200,
            keep_individual_csv=True,
        ),
    )


if __name__ == "__main__":
    main()
