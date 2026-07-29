# -*- coding: utf-8 -*-
"""Generate TVBPerf experiment JSON files with scrambled Sobol samples.

The generated JSON uses the new format:

    "base": {...},
    "samples": [{...}, {...}]

Each sample overrides ``base``.  Sobol points are generated in the renderer's
control space, not in derived G/O/Q/A space.  Discrete Sobol cells are kept
unique, then every condition point is expanded across all renderer variants.

Dependency:
    python -m pip install scipy

Usage:
    python generate_sobol_experiments.py
    python generate_sobol_experiments.py path/to/output_directory
"""

from __future__ import annotations

import json
import math
import random
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Mapping, Sequence

try:
    from scipy.stats import qmc
except ImportError as exc:  # pragma: no cover - environment-dependent
    raise SystemExit(
        "SciPy is required. Install it with: python -m pip install scipy"
    ) from exc


JsonValue = str | int | float | bool | None
Override = dict[str, JsonValue]


@dataclass(frozen=True)
class ControlDimension:
    """One independent Sobol dimension with discrete control-space options.

    Each option is an override dictionary.  This allows one dimension to move
    multiple linked renderer controls together, for example:

        {"geometry_div": 8, "object_count": 2304}
    """

    name: str
    options: tuple[Override, ...]

    def __post_init__(self) -> None:
        if not self.options:
            raise ValueError(f"Control dimension '{self.name}' has no options.")

        for index, option in enumerate(self.options):
            if not isinstance(option, dict) or not option:
                raise ValueError(
                    f"Control dimension '{self.name}' option {index} "
                    "must be a non-empty dictionary."
                )


@dataclass(frozen=True)
class ExperimentConfig:
    experiment: str
    json_filename: str
    csv_filename: str
    condition_count: int
    dimensions: tuple[ControlDimension, ...]
    base_overrides: Override
    renderer_variants: tuple[int, ...] = (1, 2, 4)
    sobol_seed: int = 0
    shuffle_seed: int = 0
    shuffle_runs: bool = True
    repeat: int = 1
    keep_individual_csv: bool = False


BASE_ARGUMENTS: Override = {
    "run_id": 0,
    "run_name": "no-run-name",
    "run_current_time": "",
    "output_filepath": "temp.csv",
    "renderer_variant": 1,
    "renderer_variant_name": "no-variant-name",
    "variable": 1,
    "to_use_scene": False,
    "scene_path": (
        "assets/scenes/unpacked/main_sponza/"
        "NewSponza_Main_glTF_003.gltf"
    ),
    "warmup_frames": 600,
    "measure_frames": 120,
    "auto_terminate": True,
    "window_width": 1280,
    "window_height": 720,
    "seed": 0,
    "camera_pos_x": 0.0,
    "camera_pos_y": 0.0,
    "camera_pos_z": -10.0,
    "camera_lookat_x": 0.0,
    "camera_lookat_y": 0.0,
    "camera_lookat_z": 0.0,
    "camera_near_z": 0.1,
    "camera_far_z": 1000.0,
    "camera_fov": 0.11,
    "object_count": 1,
    "material_count": 1,
    "geometry_count": 1,
    "z_min": -1.0,
    "z_max": 1.0,
    "xy_minmax": 1.0,
    "radius": 0.5,
    "geometry_div": 1,
    "gbuffer_cnt": 1,
    "texture_count": 1,
    "texture_size": 256,
    "texture_sampling_count": 0,
    "alu_calc_count": 0,
}

EXECUTABLE = "../out/build/x64-Release/bin/TVBPerf.exe"


def scalar_dimension(name: str, values: Sequence[JsonValue]) -> ControlDimension:
    """Create a normal one-parameter control dimension."""

    return ControlDimension(
        name=name,
        options=tuple({name: value} for value in values),
    )


def linked_dimension(
    name: str,
    options: Iterable[Mapping[str, JsonValue]],
) -> ControlDimension:
    """Create one Sobol dimension that changes linked controls together."""

    return ControlDimension(
        name=name,
        options=tuple(dict(option) for option in options),
    )


def merge_dimension_options(
    dimensions: Sequence[ControlDimension],
    option_indices: Sequence[int],
) -> Override:
    """Merge one selected option from every dimension with conflict checks."""

    merged: Override = {}

    for dimension, option_index in zip(dimensions, option_indices, strict=True):
        option = dimension.options[option_index]

        for key, value in option.items():
            if key in merged and merged[key] != value:
                raise ValueError(
                    f"Control dimensions assign conflicting values to '{key}': "
                    f"{merged[key]!r} vs {value!r}."
                )
            merged[key] = value

    return merged


def sobol_condition_points(
    dimensions: Sequence[ControlDimension],
    condition_count: int,
    seed: int,
) -> list[Override]:
    """Generate unique discrete condition points from scrambled Sobol points.

    Continuous Sobol coordinates in [0, 1) are mapped to equal-width bins for
    each discrete control dimension.  Duplicate discrete cells are skipped in
    Sobol order.  When ``condition_count`` equals the Cartesian-space size,
    every control combination appears exactly once, still in Sobol-derived
    order.
    """

    if condition_count < 1:
        raise ValueError("condition_count must be at least 1.")

    if not dimensions:
        if condition_count != 1:
            raise ValueError(
                "An experiment with no control dimensions can only have "
                "condition_count=1."
            )
        return [{}]

    cardinalities = [len(dimension.options) for dimension in dimensions]
    space_size = math.prod(cardinalities)

    if condition_count > space_size:
        raise ValueError(
            f"Requested {condition_count} unique condition points, but the "
            f"discrete control space contains only {space_size}."
        )

    # Recreate the same scrambled Sobol sequence with progressively larger
    # powers of two.  This preserves the beginning of the sequence while
    # avoiding scipy's balance warning for arbitrary sample counts.
    minimum_candidates = max(2, condition_count * 2)
    start_power = math.ceil(math.log2(minimum_candidates))
    max_candidates = max(1024, condition_count * 256, space_size * 32)
    max_power = math.ceil(math.log2(max_candidates))

    for power in range(start_power, max_power + 1):
        sampler = qmc.Sobol(
            d=len(dimensions),
            scramble=True,
            seed=seed,
        )
        unit_points = sampler.random_base2(power)

        seen_cells: set[tuple[int, ...]] = set()
        conditions: list[Override] = []

        for unit_point in unit_points:
            cell = tuple(
                min(int(float(coordinate) * cardinality), cardinality - 1)
                for coordinate, cardinality in zip(
                    unit_point,
                    cardinalities,
                    strict=True,
                )
            )

            if cell in seen_cells:
                continue

            seen_cells.add(cell)
            conditions.append(merge_dimension_options(dimensions, cell))

            if len(conditions) == condition_count:
                return conditions

    raise RuntimeError(
        "Could not obtain enough unique discrete Sobol cells. "
        "Increase the candidate limit or reduce condition_count."
    )


def expand_renderer_variants(
    conditions: Sequence[Override],
    renderer_variants: Sequence[int],
) -> list[Override]:
    """Evaluate every condition point with every requested renderer."""

    if not renderer_variants:
        raise ValueError("renderer_variants must not be empty.")

    if len(set(renderer_variants)) != len(renderer_variants):
        raise ValueError("renderer_variants contains duplicates.")

    return [
        {**condition, "renderer_variant": renderer_variant}
        for condition in conditions
        for renderer_variant in renderer_variants
    ]


def build_spec(config: ExperimentConfig) -> dict[str, Any]:
    """Build one JSON experiment specification."""

    conditions = sobol_condition_points(
        dimensions=config.dimensions,
        condition_count=config.condition_count,
        seed=config.sobol_seed,
    )
    samples = expand_renderer_variants(
        conditions,
        config.renderer_variants,
    )

    # A final deterministic shuffle removes execution-order structure while
    # leaving the Sobol-selected set unchanged.  Disable per experiment when
    # preserving Sobol order is preferred.
    if config.shuffle_runs:
        random.Random(config.shuffle_seed).shuffle(samples)

    return {
        "experiment": config.experiment,
        "executable": EXECUTABLE,
        "output": config.csv_filename,
        "repeat": config.repeat,
        "keep_individual_csv": config.keep_individual_csv,
        "base": {
            **BASE_ARGUMENTS,
            **config.base_overrides,
        },
        "samples": samples,
    }


def write_experiments(
    output_directory: Path,
    configs: Sequence[ExperimentConfig],
) -> None:
    output_directory.mkdir(parents=True, exist_ok=True)

    for config in configs:
        spec = build_spec(config)
        output_path = output_directory / config.json_filename

        with output_path.open("w", encoding="utf-8", newline="\n") as file:
            json.dump(spec, file, ensure_ascii=False, indent=2)
            file.write("\n")

        sample_count = len(spec["samples"])
        print(
            f"{output_path}: {config.condition_count} condition point(s), "
            f"{sample_count} renderer-inclusive run(s)"
        )


# ---------------------------------------------------------------------------
# Edit experiment inputs here.
#
# - condition_count counts control-space points before renderer expansion.
# - each ControlDimension consumes one Sobol coordinate.
# - use linked_dimension() when multiple renderer controls must move together.
# - Sobol scrambling and the final execution shuffle are independently seeded.
# ---------------------------------------------------------------------------
def experiment_configs(profile: str = "main") -> list[ExperimentConfig]:
    object_counts = [
        1, 2, 4, 8, 16, 32, 64, 128,
        256, 512, 1024, 2048, 4096, 8192, 16384,
    ]

    overdraw_counts = [0, 1, 2, 4, 8, 12, 16, 24, 32, 48]
    overdraw_alu_counts = [0, 1, 2, 4, 8, 12, 16, 24, 32, 48, 64]

    fixed_geometry_pairs = [
        {"geometry_div": 8, "object_count": 2304},
        {"geometry_div": 12, "object_count": 1024},
        {"geometry_div": 16, "object_count": 576},
        {"geometry_div": 24, "object_count": 256},
        {"geometry_div": 32, "object_count": 144},
        {"geometry_div": 48, "object_count": 64},
        {"geometry_div": 64, "object_count": 36},
        {"geometry_div": 96, "object_count": 16},
        {"geometry_div": 128, "object_count": 9},
        {"geometry_div": 192, "object_count": 4},
        {"geometry_div": 384, "object_count": 1},
    ]
    quad_alu_counts = [0, 1, 2, 4, 8, 16, 32, 64, 96, 128, 192]

    renderer_variants = (1, 2, 4)
    sobol_seed = 0
    shuffle_seed = 0

    if profile == "pilot":
        return [
            ExperimentConfig(
                experiment="overdraw_alu_pilot_sobol",
                json_filename="overdraw_alu_pilot_sobol.json",
                csv_filename="overdraw_alu_pilot.csv",
                condition_count=9,
                dimensions=(
                    scalar_dimension("overdraw_count", [0, 24, 48]),
                    scalar_dimension("alu_calc_count", [0, 32, 64]),
                ),
                base_overrides={
                    "object_count": 256,
                    "geometry_div": 16,
                    "texture_sampling_count": 0,
                },
                renderer_variants=renderer_variants,
                sobol_seed=sobol_seed,
                shuffle_seed=shuffle_seed,
            ),
            ExperimentConfig(
                experiment="quad_alu_pilot_sobol",
                json_filename="quad_alu_pilot_sobol.json",
                csv_filename="quad_alu_pilot.csv",
                condition_count=9,
                dimensions=(
                    linked_dimension(
                        "fixed_geometry",
                        [
                            {"geometry_div": 8, "object_count": 2304},
                            {"geometry_div": 128, "object_count": 9},
                            {"geometry_div": 384, "object_count": 1},
                        ],
                    ),
                    scalar_dimension("alu_calc_count", [0, 64, 192]),
                ),
                base_overrides={
                    "overdraw_count": 0,
                    "texture_sampling_count": 0,
                    "geometry_count": 1,
                    "material_count": 1,
                },
                renderer_variants=renderer_variants,
                sobol_seed=sobol_seed,
                shuffle_seed=shuffle_seed,
            ),
        ]

    if profile != "main":
        raise ValueError("profile must be either 'main' or 'pilot'.")

    return [
        ExperimentConfig(
            experiment="geometry_time_sobol",
            json_filename="geometry_time_sobol.json",
            csv_filename="geometry_time_orthogonal.csv",
            condition_count=15,
            dimensions=(
                scalar_dimension("object_count", object_counts),
            ),
            base_overrides={
                "geometry_div": 16,
                "overdraw_count": 0,
                "alu_calc_count": 0,
                "texture_sampling_count": 0,
            },
            renderer_variants=renderer_variants,
            sobol_seed=sobol_seed,
            shuffle_seed=shuffle_seed,
        ),
        ExperimentConfig(
            experiment="overdraw_alu_sobol",
            json_filename="overdraw_alu_sobol.json",
            csv_filename="overdraw_alu_orthogonal.csv",
            condition_count=110,
            dimensions=(
                scalar_dimension("overdraw_count", overdraw_counts),
                scalar_dimension("alu_calc_count", overdraw_alu_counts),
            ),
            base_overrides={
                "object_count": 256,
                "geometry_div": 16,
                "texture_sampling_count": 0,
            },
            renderer_variants=renderer_variants,
            sobol_seed=sobol_seed,
            shuffle_seed=shuffle_seed,
        ),
        ExperimentConfig(
            experiment="quad_alu_sobol",
            json_filename="quad_alu_sobol.json",
            csv_filename="quad_alu_orthogonal.csv",
            condition_count=121,
            dimensions=(
                linked_dimension("fixed_geometry", fixed_geometry_pairs),
                scalar_dimension("alu_calc_count", quad_alu_counts),
            ),
            base_overrides={
                "overdraw_count": 0,
                "texture_sampling_count": 0,
                "geometry_count": 1,
                "material_count": 1,
            },
            renderer_variants=renderer_variants,
            sobol_seed=sobol_seed,
            shuffle_seed=shuffle_seed,
        ),
    ]


def main() -> None:
    output_directory = (
        Path(sys.argv[1]).resolve()
        if len(sys.argv) == 2
        else Path(__file__).resolve().parent
    )

    if len(sys.argv) > 2:
        raise SystemExit(
            "Usage: python generate_sobol_experiments.py "
            "[output_directory]"
        )

    active_profile = "main"  # Change to "pilot" for the pilot JSON files.
    write_experiments(output_directory, experiment_configs(active_profile))


if __name__ == "__main__":
    main()
