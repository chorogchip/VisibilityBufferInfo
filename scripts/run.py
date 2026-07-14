# -*- coding: utf-8 -*-

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
from typing import Any, Iterator


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

    sort_type: int = 0

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


def fail(message: str) -> None:
    raise ValueError(message)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as file:
        spec = json.load(file)

    if not isinstance(spec, dict):
        fail("Experiment spec root must be an object.")

    return spec


def normalize_keys(
    values: dict[str, Any],
    section: str,
) -> dict[str, Any]:
    if not isinstance(values, dict):
        fail(f"Experiment spec '{section}' must be an object.")

    normalized = {
        normalized_key: value
        for key, value in values.items()
        if not (normalized_key := key.replace("-", "_")).startswith("_")
    }

    unknown = sorted(set(normalized) - VALID_ARGUMENTS)
    if unknown:
        fail(
            f"Unknown ProgramArgument in '{section}': "
            f"{', '.join(unknown)}"
        )

    return normalized


def render_argument(value: Any) -> str:
    if isinstance(value, bool):
        return "1" if value else "0"

    return str(value)


def command_for(
    executable: Path,
    arguments: ProgramArgument,
) -> list[str]:
    command = [str(executable)]

    for name, value in asdict(arguments).items():
        argument_name = "--" + name.replace("_", "-")
        command.extend([argument_name, render_argument(value)])

    return command


def sweep_over(
    base: dict[str, Any],
    sweep: dict[str, Any],
) -> Iterator[dict[str, Any]]:
    names = list(sweep)
    value_lists: list[list[Any]] = []

    for name in names:
        values = sweep[name]

        if not isinstance(values, list) or not values:
            fail(
                f"Sweep '{name}' must be a non-empty JSON array."
            )

        value_lists.append(values)

    for combination in itertools.product(*value_lists):
        yield {
            **base,
            **dict(zip(names, combination)),
        }


def read_single_result(path: Path) -> dict[str, str]:
    if not path.exists():
        fail(f"Benchmark output CSV was not created: {path}")

    with path.open(
        "r",
        encoding="utf-8-sig",
        newline="",
    ) as file:
        rows = list(csv.DictReader(file))

    if len(rows) != 1:
        fail(
            f"Expected exactly one benchmark row in {path}; "
            f"got {len(rows)}."
        )

    return rows[0]


def append_rows(
    path: Path,
    rows: list[dict[str, str]],
) -> None:
    if not rows:
        return

    path.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = list(rows[0].keys())
    write_header = not path.exists() or path.stat().st_size == 0

    if not write_header:
        with path.open(
            "r",
            encoding="utf-8-sig",
            newline="",
        ) as file:
            existing_fieldnames = csv.DictReader(file).fieldnames

        if existing_fieldnames != fieldnames:
            fail(f"Existing output CSV schema differs: {path}")

    with path.open(
        "a",
        encoding="utf-8",
        newline="",
    ) as file:
        writer = csv.DictWriter(
            file,
            fieldnames=fieldnames,
        )

        if write_header:
            writer.writeheader()

        writer.writerows(rows)


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(
            "Usage: python run.py path/to/experiment.json"
        )

    spec_path = Path(sys.argv[1]).resolve()
    spec = read_json(spec_path)
    spec_dir = spec_path.parent

    if "output" not in spec:
        fail("Experiment spec requires 'output'.")

    output_path = Path(str(spec["output"]))
    if not output_path.is_absolute():
        output_path = (spec_dir / output_path).resolve()

    executable = Path(
        str(
            spec.get(
                "executable",
                "../out/build/x64-Release/bin/TVBPerf.exe",
            )
        )
    )

    if not executable.is_absolute():
        executable = (spec_dir / executable).resolve()

    if not executable.exists():
        fail(f"TVBPerf executable does not exist: {executable}")

    if not executable.is_file():
        fail(f"TVBPerf executable is not a file: {executable}")

    base = normalize_keys(
        spec.get("base", {}),
        "base",
    )
    sweep = normalize_keys(
        spec.get("sweep", {}),
        "sweep",
    )

    repeat_count = int(spec.get("repeat", 1))
    if repeat_count < 1:
        fail("'repeat' must be at least 1.")

    experiment_name = str(
        spec.get("experiment", spec_path.stem)
    )
    keep_individual = bool(
        spec.get("keep_individual_csv", False)
    )

    combinations = (
        list(sweep_over(base, sweep))
        if sweep
        else [base]
    )

    # C++ ProgramArgument 구조체와 동일한 기본값으로 초기화됩니다.
    default_arguments = ProgramArgument()
    defaults = asdict(default_arguments)

    output_rows: list[dict[str, str]] = []
    temporary_dir = Path(
        tempfile.mkdtemp(prefix="tvbperf_")
    )

    try:
        total = len(combinations) * repeat_count
        run_index = 0

        for repeat in range(repeat_count):
            for combination in combinations:
                raw_path = (
                    temporary_dir
                    / f"run_{run_index:05d}.csv"
                )

                values = {
                    **defaults,
                    **combination,
                    "run_id": run_index,
                    "run_name": experiment_name,
                    "output_filepath": str(raw_path),

                    # 벤치마크 자동 실행을 위해 실행 시에만 활성화합니다.
                    # C++ 구조체의 기본값 자체는 False로 유지됩니다.
                    "auto_terminate": True,
                }

                arguments = ProgramArgument(**values)
                command = command_for(
                    executable,
                    arguments,
                )

                print(
                    f"[{run_index + 1}/{total}] "
                    f"{subprocess.list2cmdline(command)}"
                )

                subprocess.run(
                    command,
                    cwd=executable.parent,
                    check=True,
                )

                result = read_single_result(raw_path)

                output_rows.append(
                    {
                        "experiment": experiment_name,
                        "repeat": str(repeat),
                        "run_index": str(run_index),
                        **result,
                    }
                )

                if keep_individual:
                    individual_dir = (
                        output_path.parent
                        / f"{output_path.stem}_runs"
                    )
                    individual_dir.mkdir(
                        parents=True,
                        exist_ok=True,
                    )
                    shutil.copy2(
                        raw_path,
                        individual_dir / raw_path.name,
                    )

                run_index += 1

    finally:
        shutil.rmtree(
            temporary_dir,
            ignore_errors=True,
        )

    append_rows(output_path, output_rows)

    print(
        f"Appended {len(output_rows)} benchmark row(s): "
        f"{output_path}"
    )


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python run.py path/to/experiment.json")

    spec_path = Path(sys.argv[1]).resolve()
    spec = read_json(spec_path)
    spec_dir = spec_path.parent

    # JSON의 output에는 파일 이름만 지정:
    # "output": "geometry_sweep.csv"
    output_name = Path(str(spec["output"])).name
    output_stem = Path(output_name).stem

    # .\results\geometry_sweep\geometry_sweep.csv
    output_dir = spec_dir / "results" / output_stem
    output_dir.mkdir(parents=True, exist_ok=True)

    output_path = output_dir / output_name

    executable = Path(
        str(spec.get(
            "executable",
            "../out/build/x64-Release/bin/TVBPerf.exe",
        ))
    )

    if not executable.is_absolute():
        executable = (spec_dir / executable).resolve()

    base = normalize_keys(spec.get("base", {}), "base")
    sweep = normalize_keys(spec.get("sweep", {}), "sweep")

    repeat_count = int(spec.get("repeat", 1))
    experiment_name = str(spec.get("experiment", spec_path.stem))
    keep_individual = bool(spec.get("keep_individual_csv", False))

    combinations = (
        list(sweep_over(base, sweep))
        if sweep
        else [base]
    )

    defaults = asdict(ProgramArgument())
    output_rows: list[dict[str, str]] = []

    temporary_dir = Path(tempfile.mkdtemp(prefix="tvbperf_"))

    try:
        total = len(combinations) * repeat_count
        run_index = 0

        for repeat in range(repeat_count):
            for combination in combinations:
                raw_path = temporary_dir / f"run_{run_index:05d}.csv"

                values = {
                    **defaults,
                    **combination,
                    "run_id": run_index,
                    "run_name": experiment_name,
                    "output_filepath": str(raw_path),
                    "auto_terminate": True,
                }

                arguments = ProgramArgument(**values)
                command = command_for(executable, arguments)

                print(f"[{run_index + 1}/{total}] {' '.join(command)}")

                subprocess.run(
                    command,
                    cwd=executable.parent,
                    check=True,
                )

                output_rows.append({
                    "experiment": experiment_name,
                    "repeat": str(repeat),
                    "run_index": str(run_index),
                    **read_single_result(raw_path),
                })

                if keep_individual:
                    individual_dir = output_dir / f"{output_stem}_runs"
                    individual_dir.mkdir(parents=True, exist_ok=True)

                    shutil.copy2(
                        raw_path,
                        individual_dir / raw_path.name,
                    )

                run_index += 1

    finally:
        shutil.rmtree(temporary_dir, ignore_errors=True)

    append_rows(output_path, output_rows)

    # 실행에 사용한 JSON도 결과 폴더에 복사
    shutil.copy2(
        spec_path,
        output_dir / spec_path.name,
    )

    print(f"CSV: {output_path}")
    print(f"JSON: {output_dir / spec_path.name}")


if __name__ == "__main__":
    main()