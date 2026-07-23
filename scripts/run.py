# -*- coding: utf-8 -*-

"""Standalone TVBPerf benchmark sweep runner.

Usage:
    python run.py path/to/experiment.json

This file contains all functionality that previously lived in script_libs.
It checkpoints every run, preserves partial results, and copies per-run CSV
artifacts out of the temporary directory before that directory is deleted.
"""

from __future__ import annotations

import csv
import itertools
import json
import shutil
import subprocess
import sys
import tempfile
import traceback
from dataclasses import asdict, dataclass, fields
from datetime import datetime
from pathlib import Path
from typing import Any, Iterator


ERROR_TEXT_LIMIT = 8000
PROGRAM_RESULT_PASS_COUNT = 32


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


def now_iso() -> str:
    return datetime.now().astimezone().isoformat(timespec="seconds")


def fail(message: str) -> None:
    raise ValueError(message)


def render_argument(value: Any) -> str:
    if isinstance(value, bool):
        return "1" if value else "0"
    return str(value)


def trim_error(value: Any) -> str:
    text = str(value or "").strip()
    if len(text) <= ERROR_TEXT_LIMIT:
        return text
    return text[-ERROR_TEXT_LIMIT:]


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as file:
        spec = json.load(file)
    if not isinstance(spec, dict):
        fail("Experiment spec root must be an object.")
    return spec


def normalize_keys(values: dict[str, Any], section: str) -> dict[str, Any]:
    if not isinstance(values, dict):
        fail(f"Experiment spec '{section}' must be an object.")

    normalized = {
        normalized_key: value
        for key, value in values.items()
        if not (normalized_key := key.replace("-", "_")).startswith("_")
    }
    unknown = sorted(set(normalized) - VALID_ARGUMENTS)
    if unknown:
        fail(f"Unknown ProgramArgument in '{section}': {', '.join(unknown)}")
    return normalized


def sweep_over(base: dict[str, Any], sweep: dict[str, Any]) -> Iterator[dict[str, Any]]:
    names = list(sweep)
    value_lists: list[list[Any]] = []
    for name in names:
        values = sweep[name]
        if not isinstance(values, list) or not values:
            fail(f"Sweep '{name}' must be a non-empty JSON array.")
        value_lists.append(values)

    for combination in itertools.product(*value_lists):
        yield {**base, **dict(zip(names, combination))}


def samples_over(base: dict[str, Any], samples: Any) -> Iterator[dict[str, Any]]:
    if not isinstance(samples, list) or not samples:
        fail("Experiment spec 'samples' must be a non-empty JSON array.")

    for sample_index, sample in enumerate(samples):
        normalized = normalize_keys(sample, f"samples[{sample_index}]")
        yield {**base, **normalized}


def parameter_sets(spec: dict[str, Any]) -> tuple[list[dict[str, Any]], str]:
    base = normalize_keys(spec.get("base", {}), "base")
    sweep = normalize_keys(spec.get("sweep", {}), "sweep")
    has_samples = "samples" in spec

    if has_samples and sweep:
        fail("Experiment spec cannot use both non-empty 'sweep' and 'samples'.")
    if has_samples:
        return list(samples_over(base, spec["samples"])), "samples"
    if sweep:
        return list(sweep_over(base, sweep)), "sweep"
    return [base], "base"


def result_paths(spec_path: Path) -> tuple[Path, Path, Path, Path]:
    output_name = f"{spec_path.stem}.csv"
    output_dir = spec_path.parent / "results" / spec_path.stem
    return (
        output_dir,
        output_dir / output_name,
        output_dir / spec_path.name,
        output_dir / f"{spec_path.stem}_run_report.json",
    )


def copy_if_possible(source: Path, destination: Path) -> str:
    try:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        return ""
    except Exception as error:
        return trim_error(error)


def discover_related_run_files(raw_path: Path) -> list[Path]:
    """Find sidecar files emitted next to one temporary benchmark CSV.

    Supports both the current C++ naming form:
        run_00000.csv_0_result.csv

    and the cleaner stem-based form:
        run_00000_0_result.csv
        run_00000_scene_fingerprint.csv
    """
    if not raw_path.parent.exists():
        return []

    matches: list[Path] = []
    raw_name_prefix = raw_path.name + "_"
    stem_prefix = raw_path.stem + "_"

    for candidate in raw_path.parent.iterdir():
        if not candidate.is_file() or candidate == raw_path:
            continue
        if candidate.name.startswith(raw_name_prefix) or candidate.name.startswith(stem_prefix):
            matches.append(candidate)

    return sorted(matches, key=lambda item: item.name)


def copy_run_files(
    raw_path: Path,
    destination_dir: Path,
    should_copy: bool,
) -> tuple[str, str, list[str], list[str]]:
    """Copy the main CSV and every related sidecar before temp cleanup."""
    individual_csv = ""
    individual_copy_error = ""
    artifact_csvs: list[str] = []
    artifact_copy_errors: list[str] = []

    if not should_copy:
        return (
            individual_csv,
            individual_copy_error,
            artifact_csvs,
            artifact_copy_errors,
        )

    if raw_path.exists():
        destination = destination_dir / raw_path.name
        individual_copy_error = copy_if_possible(raw_path, destination)
        if individual_copy_error:
            artifact_copy_errors.append(
                f"{raw_path.name}: {individual_copy_error}"
            )
        else:
            individual_csv = str(destination)

    for source in discover_related_run_files(raw_path):
        destination = destination_dir / source.name
        copy_error = copy_if_possible(source, destination)
        if copy_error:
            artifact_copy_errors.append(f"{source.name}: {copy_error}")
        else:
            artifact_csvs.append(str(destination))

    return (
        individual_csv,
        individual_copy_error,
        artifact_csvs,
        artifact_copy_errors,
    )


def read_result_rows(path: Path) -> list[dict[str, str]]:
    if not path.exists() or path.stat().st_size == 0:
        return []

    with path.open("r", encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        if not reader.fieldnames:
            fail(f"Benchmark output CSV has no header: {path}")

        missing_fields = [
            field_name
            for field_name in PROGRAM_RESULT_FIELDS
            if field_name not in reader.fieldnames
        ]
        if missing_fields:
            fail(
                "Benchmark output CSV is missing ProgramResult fields: "
                + ", ".join(missing_fields)
            )

        rows: list[dict[str, str]] = []
        for row in reader:
            normalized = {
                str(key): "" if value is None else str(value)
                for key, value in row.items()
                if key is not None
            }
            if any(value != "" for value in normalized.values()):
                rows.append(normalized)
        return rows


def append_rows_flexible(path: Path, rows: list[dict[str, Any]]) -> None:
    if not rows:
        return

    path.parent.mkdir(parents=True, exist_ok=True)
    string_rows = [
        {str(key): render_argument(value) for key, value in row.items()}
        for row in rows
    ]
    existing_rows: list[dict[str, str]] = []
    existing_fieldnames: list[str] = []

    if path.exists() and path.stat().st_size > 0:
        try:
            with path.open("r", encoding="utf-8-sig", newline="") as file:
                reader = csv.DictReader(file)
                existing_fieldnames = list(reader.fieldnames or [])
                existing_rows = [dict(row) for row in reader]
        except Exception as error:
            stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            backup = path.with_name(f"{path.stem}.corrupt_{stamp}{path.suffix}")
            shutil.move(path, backup)
            print(
                f"WARNING: Existing CSV could not be read and was moved to "
                f"{backup}: {error}",
                file=sys.stderr,
            )

    fieldnames = list(existing_fieldnames)
    for row in string_rows:
        for key in row:
            if key not in fieldnames:
                fieldnames.append(key)

    schema_changed = fieldnames != existing_fieldnames
    file_is_empty = not path.exists() or path.stat().st_size == 0
    if file_is_empty or schema_changed:
        temporary_path = path.with_suffix(path.suffix + ".tmp")
        with temporary_path.open("w", encoding="utf-8-sig", newline="") as file:
            writer = csv.DictWriter(
                file,
                fieldnames=fieldnames,
                extrasaction="ignore",
            )
            writer.writeheader()
            writer.writerows(existing_rows)
            writer.writerows(string_rows)
        temporary_path.replace(path)
        return

    with path.open("a", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(
            file,
            fieldnames=fieldnames,
            extrasaction="ignore",
        )
        writer.writerows(string_rows)


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = path.with_suffix(path.suffix + ".tmp")
    with temporary_path.open("w", encoding="utf-8") as file:
        json.dump(value, file, ensure_ascii=False, indent=2)
        file.write("\n")
    temporary_path.replace(path)


def safe_write_json(path: Path, value: dict[str, Any]) -> bool:
    try:
        write_json_atomic(path, value)
        return True
    except Exception as error:
        print(f"WARNING: Could not write JSON report {path}: {error}", file=sys.stderr)
        return False


def command_for(executable: Path, arguments: ProgramArgument) -> list[str]:
    command = [str(executable)]
    for name, value in asdict(arguments).items():
        command.extend(["--" + name.replace("_", "-"), render_argument(value)])
    return command


def execute(
    command: list[str],
    cwd: Path,
    timeout_seconds: float | None,
) -> tuple[int | None, str, str, bool]:
    """Return return_code, process_error, stderr_text, interrupted."""
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            check=False,
            stderr=subprocess.PIPE,
            text=True,
            errors="replace",
            timeout=timeout_seconds,
        )
        stderr_text = trim_error(completed.stderr)
        process_error = (
            ""
            if completed.returncode == 0
            else f"Process exited with code {completed.returncode}."
        )
        return completed.returncode, process_error, stderr_text, False
    except subprocess.TimeoutExpired as error:
        return (
            None,
            f"Process timed out after {timeout_seconds} second(s).",
            trim_error(error.stderr),
            False,
        )
    except KeyboardInterrupt:
        return None, "Interrupted by user.", "", True
    except Exception as error:
        return None, f"Could not execute benchmark: {error}", "", False


def build_csv_rows(
    *,
    raw_rows: list[dict[str, str]],
    combination: dict[str, Any],
    experiment_name: str,
    repeat: int,
    run_index: int,
    return_code: int | None,
    combined_error: str,
    raw_path: Path,
    report: dict[str, Any],
) -> tuple[str, list[dict[str, Any]]]:
    """Apply the runner's success/salvage/failure policy to one run."""
    parameter_columns = {
        f"param_{name}": render_argument(value)
        for name, value in combination.items()
    }

    if raw_rows:
        run_status = "salvaged" if combined_error else "success"
        counter_key = (
            "successful_runs" if run_status == "success" else "salvaged_runs"
        )
        report[counter_key] += 1
        rows = [
            {
                **result_row,
                **parameter_columns,
                "runner_experiment": experiment_name,
                "runner_repeat": repeat,
                "runner_run_index": run_index,
                "runner_result_row": result_row_index,
                "runner_status": run_status,
                "runner_return_code": "" if return_code is None else return_code,
                "runner_error": combined_error,
            }
            for result_row_index, result_row in enumerate(raw_rows)
        ]
        return run_status, rows

    report["failed_runs"] += 1
    no_result_error = combined_error or (
        f"Benchmark output CSV was not created or had no rows: {raw_path}"
    )
    return "failed", [
        {
            **parameter_columns,
            "runner_experiment": experiment_name,
            "runner_repeat": repeat,
            "runner_run_index": run_index,
            "runner_result_row": "",
            "runner_status": "failed",
            "runner_return_code": "" if return_code is None else return_code,
            "runner_error": no_result_error,
        }
    ]


def run_experiment(
    spec_path: Path,
    spec: dict[str, Any],
    output_dir: Path,
    output_csv: Path,
    report_json: Path,
    report: dict[str, Any],
) -> int:
    """Execute every parameter set and checkpoint each run immediately."""
    executable = Path(
        str(spec.get("executable", "../out/build/x64-Release/bin/TVBPerf.exe"))
    )
    if not executable.is_absolute():
        executable = (spec_path.parent / executable).resolve()

    combinations, parameter_source = parameter_sets(spec)
    repeat_count = int(spec.get("repeat", 1))
    if repeat_count < 1:
        fail("'repeat' must be at least 1.")

    timeout_value = spec.get("timeout_seconds")
    timeout_seconds = None if timeout_value is None else float(timeout_value)
    if timeout_seconds is not None and timeout_seconds <= 0:
        fail("'timeout_seconds' must be greater than 0.")

    experiment_name = spec_path.stem
    keep_individual = bool(spec.get("keep_individual_csv", False))
    total = len(combinations) * repeat_count

    report.update(
        {
            "experiment": experiment_name,
            "executable": str(executable),
            "parameter_source": parameter_source,
            "parameter_set_count": len(combinations),
            "total_runs": total,
            "successful_runs": 0,
            "salvaged_runs": 0,
            "failed_runs": 0,
        }
    )
    safe_write_json(report_json, report)

    if not executable.exists() or not executable.is_file():
        fail(f"TVBPerf executable does not exist or is not a file: {executable}")

    defaults = asdict(ProgramArgument())
    individual_dir = output_dir / f"{output_csv.stem}_runs"
    temporary_dir = Path(tempfile.mkdtemp(prefix="tvbperf_"))
    run_index = 0

    try:
        for repeat in range(repeat_count):
            for combination in combinations:
                raw_path = temporary_dir / f"run_{run_index:05d}.csv"
                arguments = ProgramArgument(
                    **{
                        **defaults,
                        **combination,
                        "run_id": run_index,
                        "run_name": experiment_name,
                        "output_filepath": str(raw_path),
                        "auto_terminate": True,
                    }
                )
                command = command_for(executable, arguments)
                print(f"[{run_index + 1}/{total}] {subprocess.list2cmdline(command)}")

                started_at = now_iso()
                return_code, process_error, stderr_text, interrupted = execute(
                    command,
                    executable.parent,
                    timeout_seconds,
                )
                if stderr_text:
                    print(stderr_text, file=sys.stderr)

                read_error = ""
                try:
                    raw_rows = read_result_rows(raw_path)
                except Exception as error:
                    raw_rows = []
                    read_error = f"Could not read benchmark CSV: {error}"

                combined_error = " | ".join(
                    part
                    for part in (process_error, read_error, stderr_text)
                    if part
                )
                run_status, csv_rows = build_csv_rows(
                    raw_rows=raw_rows,
                    combination=combination,
                    experiment_name=experiment_name,
                    repeat=repeat,
                    run_index=run_index,
                    return_code=return_code,
                    combined_error=combined_error,
                    raw_path=raw_path,
                    report=report,
                )

                csv_write_error = ""
                try:
                    append_rows_flexible(output_csv, csv_rows)
                except Exception as error:
                    csv_write_error = trim_error(error)
                    print(
                        f"ERROR: Could not append run {run_index} to "
                        f"{output_csv}: {csv_write_error}",
                        file=sys.stderr,
                    )

                should_copy = keep_individual or run_status != "success"
                (
                    individual_csv,
                    individual_copy_error,
                    artifact_csvs,
                    artifact_copy_errors,
                ) = copy_run_files(raw_path, individual_dir, should_copy)

                if artifact_csvs:
                    print("  -> copied sidecar CSV(s):")
                    for artifact_path in artifact_csvs:
                        print(f"     {artifact_path}")
                for copy_error in artifact_copy_errors:
                    print(f"WARNING: Could not copy run artifact: {copy_error}", file=sys.stderr)

                report["runs"].append(
                    {
                        "run_index": run_index,
                        "repeat": repeat,
                        "status": run_status,
                        "started_at": started_at,
                        "finished_at": now_iso(),
                        "return_code": return_code,
                        "command": command,
                        "parameters": combination,
                        "raw_csv": str(raw_path) if raw_path.exists() else None,
                        "individual_csv": individual_csv or None,
                        "artifact_csvs": artifact_csvs,
                        "raw_row_count": len(raw_rows),
                        "error": combined_error or None,
                        "csv_write_error": csv_write_error or None,
                        "individual_copy_error": individual_copy_error or None,
                        "artifact_copy_errors": artifact_copy_errors,
                    }
                )
                report["completed_runs"] = len(report["runs"])
                report["last_updated_at"] = now_iso()
                safe_write_json(report_json, report)

                print(
                    f"  -> {run_status}: {len(raw_rows)} result row(s)"
                    + (f"; {combined_error}" if combined_error else "")
                )

                run_index += 1
                if interrupted:
                    raise KeyboardInterrupt
    finally:
        shutil.rmtree(temporary_dir, ignore_errors=True)

    return 1 if report["failed_runs"] or report["salvaged_runs"] else 0


def initial_report(
    spec_path: Path,
    spec_copy: Path,
    output_csv: Path,
    report_json: Path,
) -> dict[str, Any]:
    timestamp = now_iso()
    return {
        "status": "starting",
        "started_at": timestamp,
        "last_updated_at": timestamp,
        "finished_at": None,
        "spec_path": str(spec_path),
        "spec_copy": str(spec_copy),
        "output_csv": str(output_csv),
        "report_json": str(report_json),
        "completed_runs": 0,
        "successful_runs": 0,
        "salvaged_runs": 0,
        "failed_runs": 0,
        "runs": [],
        "fatal_error": None,
        "spec_copy_error": None,
    }


def handle_fatal_error(
    error: Exception,
    spec_path: Path,
    spec_copy: Path,
    output_csv: Path,
    report: dict[str, Any],
) -> int:
    report["status"] = "fatal_error"
    report["fatal_error"] = trim_error("".join(traceback.format_exception(error)))
    report["failed_runs"] = int(report.get("failed_runs", 0)) + 1

    if not spec_copy.exists():
        report["spec_copy_error"] = copy_if_possible(spec_path, spec_copy) or None

    fatal_run_index = int(report.get("completed_runs", 0))
    report["runs"].append(
        {
            "run_index": fatal_run_index,
            "repeat": None,
            "status": "fatal_error",
            "started_at": None,
            "finished_at": now_iso(),
            "return_code": None,
            "command": None,
            "parameters": None,
            "raw_csv": None,
            "individual_csv": None,
            "artifact_csvs": [],
            "raw_row_count": 0,
            "error": str(error),
            "csv_write_error": None,
            "individual_copy_error": None,
            "artifact_copy_errors": [],
        }
    )

    try:
        append_rows_flexible(
            output_csv,
            [
                {
                    "runner_status": "fatal_error",
                    "runner_experiment": spec_path.stem,
                    "runner_repeat": "",
                    "runner_run_index": fatal_run_index,
                    "runner_result_row": "",
                    "runner_return_code": "",
                    "runner_error": str(error),
                }
            ],
        )
    except Exception as csv_error:
        report["fatal_csv_write_error"] = trim_error(csv_error)

    print(report["fatal_error"], file=sys.stderr)
    return 1


def print_summary(
    output_csv: Path,
    spec_copy: Path,
    report_json: Path,
    report: dict[str, Any],
) -> None:
    print(f"CSV: {output_csv}")
    print(f"Input JSON copy: {spec_copy}")
    print(f"Run report JSON: {report_json}")
    print(
        "Runs: "
        f"success={report.get('successful_runs', 0)}, "
        f"salvaged={report.get('salvaged_runs', 0)}, "
        f"failed={report.get('failed_runs', 0)}"
    )


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: python run.py path/to/experiment.json", file=sys.stderr)
        return 2

    spec_path = Path(sys.argv[1]).resolve()
    output_dir, output_csv, spec_copy, report_json = result_paths(spec_path)
    report = initial_report(spec_path, spec_copy, output_csv, report_json)

    exit_code = 1
    try:
        spec = read_json(spec_path)
        output_dir, output_csv, spec_copy, report_json = result_paths(spec_path)
        output_dir.mkdir(parents=True, exist_ok=True)
        report.update(
            {
                "status": "running",
                "last_updated_at": now_iso(),
                "spec_copy": str(spec_copy),
                "output_csv": str(output_csv),
                "report_json": str(report_json),
            }
        )
        report["spec_copy_error"] = copy_if_possible(spec_path, spec_copy) or None
        safe_write_json(report_json, report)

        exit_code = run_experiment(
            spec_path,
            spec,
            output_dir,
            output_csv,
            report_json,
            report,
        )
        report["status"] = (
            "completed" if exit_code == 0 else "completed_with_errors"
        )
    except KeyboardInterrupt:
        report["status"] = "interrupted"
        report["fatal_error"] = "Interrupted by user."
        exit_code = 130
    except Exception as error:
        exit_code = handle_fatal_error(
            error,
            spec_path,
            spec_copy,
            output_csv,
            report,
        )
    finally:
        report["finished_at"] = now_iso()
        report["last_updated_at"] = report["finished_at"]
        safe_write_json(report_json, report)
        print_summary(output_csv, spec_copy, report_json, report)

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
