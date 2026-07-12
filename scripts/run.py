"""Run a benchmark sweep described by JSON and append all runs into one CSV.

Usage:
    python run.py path/to/experiment.json
"""

from __future__ import annotations

import csv
import itertools
import json
import shutil
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass, fields
from pathlib import Path
from typing import Any


@dataclass
class ProgramArgument:
    run_id: int = 0
    run_name: str = "no-run-name"
    run_current_time: str = ""
    output_filepath: str = "temp.csv"
    renderer_variant: int = 4
    renderer_variant_name: str = "no-variant-name"
    variable: int = 0
    to_use_scene: bool = False
    scene_path: str = "assets/scenes/unpacked/CornellBox/CornellBox-Empty-RG.obj"
    warmup_frames: int = 1200
    measure_frames: int = 120
    auto_terminate: bool = True
    window_width: int = 1280
    window_height: int = 720
    seed: int = 0
    camera_pos_x: float = 1.0
    camera_pos_y: float = 0.1
    camera_pos_z: float = -4.0
    camera_lookat_x: float = 0.0
    camera_lookat_y: float = 0.0
    camera_lookat_z: float = 0.0
    camera_near_z: float = 0.1
    camera_far_z: float = 1000.0
    camera_fov: float = 45.0
    sphere_count: int = 50
    material_count: int = 1
    geometry_count: int = 1
    z_min: float = -1.0
    z_max: float = 1.0
    xy_minmax: float = 1.0
    radius: float = 0.1
    geometry_div: int = 10
    gbuffer_cnt: int = 1
    texture_count: int = 1
    texture_size: int = 1
    texture_sampling_count: int = 0
    alu_calc_count: int = 0


VALID_ARGUMENTS = {field.name for field in fields(ProgramArgument)}


def fail(message: str) -> None:
    raise ValueError(message)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as file:
        spec = json.load(file)
    if not isinstance(spec, dict):
        fail("Experiment spec root must be an object.")
    return spec


def normalize_keys(values: dict[str, Any], section: str) -> dict[str, Any]:
    normalized = {key.replace("-", "_"): value for key, value in values.items()}
    unknown = sorted(set(normalized) - VALID_ARGUMENTS)
    if unknown:
        fail(f"Unknown ProgramArgument in '{section}': {', '.join(unknown)}")
    return normalized


def command_for(executable: Path, arguments: ProgramArgument) -> list[str]:
    command = [str(executable)]
    for name, value in asdict(arguments).items():
        rendered = "1" if value is True else "0" if value is False else str(value)
        command.extend(["--" + name.replace("_", "-"), rendered])
    return command


def sweep_over(base: dict[str, Any], sweep: dict[str, Any]):
    names = list(sweep)
    value_lists: list[list[Any]] = []
    for name in names:
        values = sweep[name]
        if not isinstance(values, list) or not values:
            fail(f"Sweep '{name}' must be a non-empty JSON array.")
        value_lists.append(values)
    for combination in itertools.product(*value_lists):
        yield {**base, **dict(zip(names, combination))}


def read_single_result(path: Path) -> dict[str, str]:
    with path.open("r", encoding="utf-8-sig", newline="") as file:
        rows = list(csv.DictReader(file))
    if len(rows) != 1:
        fail(f"Expected exactly one benchmark row in {path}; got {len(rows)}.")
    return rows[0]


def append_rows(path: Path, rows: list[dict[str, str]]) -> None:
    if not rows:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = list(rows[0].keys())
    write_header = not path.exists() or path.stat().st_size == 0
    if not write_header:
        with path.open("r", encoding="utf-8-sig", newline="") as file:
            if csv.DictReader(file).fieldnames != fieldnames:
                fail(f"Existing output CSV schema differs: {path}")
    with path.open("a", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        if write_header:
            writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python run.py path/to/experiment.json")
    spec_path = Path(sys.argv[1]).resolve()
    spec = read_json(spec_path)
    spec_dir = spec_path.parent
    if "output" not in spec:
        fail("Experiment spec requires 'output'.")
    output_path = Path(str(spec["output"]))
    if not output_path.is_absolute():
        output_path = spec_dir / output_path
    executable = Path(str(spec.get("executable", "../out/build/x64-Release/bin/TVBPerf.exe")))
    if not executable.is_absolute():
        executable = (spec_dir / executable).resolve()
    if not executable.exists():
        fail(f"TVBPerf executable does not exist: {executable}")
    base = normalize_keys(spec.get("base", {}), "base")
    sweep = normalize_keys(spec.get("sweep", {}), "sweep")
    repeat_count = int(spec.get("repeat", 1))
    if repeat_count < 1:
        fail("'repeat' must be at least 1.")
    experiment_name = str(spec.get("experiment", spec_path.stem))
    keep_individual = bool(spec.get("keep_individual_csv", False))
    combinations = list(sweep_over(base, sweep)) if sweep else [base]
    defaults = asdict(ProgramArgument())
    output_rows: list[dict[str, str]] = []
    temporary_dir = Path(tempfile.mkdtemp(prefix="tvbperf_"))
    try:
        total = len(combinations) * repeat_count
        run_index = 0
        for repeat in range(repeat_count):
            for combination in combinations:
                raw_path = temporary_dir / f"run_{run_index:05d}.csv"
                values = {**defaults, **combination, "run_id": run_index, "run_name": experiment_name,
                          "output_filepath": str(raw_path), "auto_terminate": True}
                arguments = ProgramArgument(**values)
                command = command_for(executable, arguments)
                print(f"[{run_index + 1}/{total}] {' '.join(command)}")
                subprocess.run(command, cwd=executable.parent, check=True)
                output_rows.append({"experiment": experiment_name, "repeat": str(repeat),
                                    "run_index": str(run_index), **read_single_result(raw_path)})
                if keep_individual:
                    individual_dir = output_path.parent / (output_path.stem + "_runs")
                    individual_dir.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(raw_path, individual_dir / raw_path.name)
                run_index += 1
    finally:
        shutil.rmtree(temporary_dir, ignore_errors=True)
    append_rows(output_path, output_rows)
    print(f"Appended {len(output_rows)} benchmark row(s): {output_path}")


if __name__ == "__main__":
    main()
