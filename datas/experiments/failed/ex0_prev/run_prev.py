# -*- coding: utf-8 -*-

"""Run a benchmark sweep and preserve as much CSV/JSON output as possible.

Usage:
    python run_resilient.py path/to/experiment.json

Behavior:
- A failure in one benchmark run does not stop the remaining sweep.
- Every run writes a success/failure row to the combined CSV immediately.
- Rows produced before an executable error are still salvaged.
- The input JSON is copied to the result directory as early as possible.
- A checkpoint JSON report is updated after every run.
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
ERROR_TEXT_LIMIT = 8000


def now_iso() -> str:
    return datetime.now().astimezone().isoformat(timespec="seconds")


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
            fail(f"Sweep '{name}' must be a non-empty JSON array.")
        value_lists.append(values)

    for combination in itertools.product(*value_lists):
        yield {**base, **dict(zip(names, combination))}


def trim_error(value: Any) -> str:
    text = str(value or "").strip()
    if len(text) <= ERROR_TEXT_LIMIT:
        return text
    return text[-ERROR_TEXT_LIMIT:]


def read_result_rows(path: Path) -> list[dict[str, str]]:
    """Read every usable row so partially written output can be salvaged."""

    if not path.exists() or path.stat().st_size == 0:
        return []

    with path.open("r", encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        if not reader.fieldnames:
            fail(f"Benchmark output CSV has no header: {path}")

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


def append_rows_flexible(
    path: Path,
    rows: list[dict[str, Any]],
) -> None:
    """Append rows and expand the existing schema when new columns appear."""

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
            existing_rows = []
            existing_fieldnames = []

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


def result_paths(
    spec_path: Path,
    spec: dict[str, Any] | None,
) -> tuple[Path, Path, Path, Path]:
    fallback_output = f"{spec_path.stem}.csv"
    output_value = fallback_output

    if spec is not None and "output" in spec:
        output_value = str(spec["output"])

    output_name = Path(output_value).name or fallback_output
    if Path(output_name).suffix.lower() != ".csv":
        output_name = f"{Path(output_name).stem or spec_path.stem}.csv"

    output_stem = Path(output_name).stem
    output_dir = spec_path.parent / "results" / output_stem
    output_csv = output_dir / output_name
    spec_copy = output_dir / spec_path.name
    report_json = output_dir / f"{output_stem}_run_report.json"
    return output_dir, output_csv, spec_copy, report_json


def copy_if_possible(source: Path, destination: Path) -> str:
    try:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        return ""
    except Exception as error:
        return trim_error(error)


def run_experiment(
    spec_path: Path,
    spec: dict[str, Any],
    output_dir: Path,
    output_csv: Path,
    report_json: Path,
    report: dict[str, Any],
) -> int:
    spec_dir = spec_path.parent

    if "output" not in spec:
        fail("Experiment spec requires 'output'.")

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

    base = normalize_keys(spec.get("base", {}), "base")
    sweep = normalize_keys(spec.get("sweep", {}), "sweep")

    repeat_count = int(spec.get("repeat", 1))
    if repeat_count < 1:
        fail("'repeat' must be at least 1.")

    timeout_value = spec.get("timeout_seconds")
    timeout_seconds: float | None = None
    if timeout_value is not None:
        timeout_seconds = float(timeout_value)
        if timeout_seconds <= 0:
            fail("'timeout_seconds' must be greater than 0.")

    experiment_name = str(spec.get("experiment", spec_path.stem))
    keep_individual = bool(spec.get("keep_individual_csv", False))
    combinations = list(sweep_over(base, sweep)) if sweep else [base]
    defaults = asdict(ProgramArgument())
    total = len(combinations) * repeat_count

    report.update(
        {
            "experiment": experiment_name,
            "executable": str(executable),
            "total_runs": total,
            "successful_runs": 0,
            "salvaged_runs": 0,
            "failed_runs": 0,
        }
    )
    safe_write_json(report_json, report)

    if not executable.exists() or not executable.is_file():
        fail(f"TVBPerf executable does not exist or is not a file: {executable}")

    individual_dir = output_dir / f"{output_csv.stem}_runs"
    temporary_dir = Path(tempfile.mkdtemp(prefix="tvbperf_"))
    run_index = 0

    try:
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
                command_text = subprocess.list2cmdline(command)

                print(f"[{run_index + 1}/{total}] {command_text}")

                started_at = now_iso()
                return_code: int | None = None
                process_error = ""
                stderr_text = ""
                read_error = ""
                raw_rows: list[dict[str, str]] = []
                interrupted = False

                try:
                    completed = subprocess.run(
                        command,
                        cwd=executable.parent,
                        check=False,
                        stderr=subprocess.PIPE,
                        text=True,
                        errors="replace",
                        timeout=timeout_seconds,
                    )
                    return_code = completed.returncode
                    stderr_text = trim_error(completed.stderr)
                    if stderr_text:
                        print(stderr_text, file=sys.stderr)
                    if return_code != 0:
                        process_error = f"Process exited with code {return_code}."
                except subprocess.TimeoutExpired as error:
                    process_error = (
                        f"Process timed out after {timeout_seconds} second(s)."
                    )
                    stderr_text = trim_error(error.stderr)
                except KeyboardInterrupt:
                    process_error = "Interrupted by user."
                    interrupted = True
                except Exception as error:
                    process_error = f"Could not execute benchmark: {error}"

                try:
                    raw_rows = read_result_rows(raw_path)
                except Exception as error:
                    read_error = f"Could not read benchmark CSV: {error}"

                combined_error = " | ".join(
                    part
                    for part in (process_error, read_error, stderr_text)
                    if part
                )

                if raw_rows:
                    if process_error or read_error:
                        run_status = "salvaged"
                        report["salvaged_runs"] += 1
                    else:
                        run_status = "success"
                        report["successful_runs"] += 1

                    csv_rows: list[dict[str, Any]] = []
                    for result_row_index, result_row in enumerate(raw_rows):
                        csv_rows.append(
                            {
                                **result_row,
                                **{
                                    f"param_{name}": render_argument(value)
                                    for name, value in combination.items()
                                },
                                "runner_experiment": experiment_name,
                                "runner_repeat": repeat,
                                "runner_run_index": run_index,
                                "runner_result_row": result_row_index,
                                "runner_status": run_status,
                                "runner_return_code": (
                                    "" if return_code is None else return_code
                                ),
                                "runner_error": combined_error,
                            }
                        )
                else:
                    run_status = "failed"
                    report["failed_runs"] += 1
                    no_result_error = combined_error or (
                        f"Benchmark output CSV was not created or had no rows: "
                        f"{raw_path}"
                    )
                    csv_rows = [
                        {
                            **{
                                f"param_{name}": render_argument(value)
                                for name, value in combination.items()
                            },
                            "runner_experiment": experiment_name,
                            "runner_repeat": repeat,
                            "runner_run_index": run_index,
                            "runner_result_row": "",
                            "runner_status": run_status,
                            "runner_return_code": (
                                "" if return_code is None else return_code
                            ),
                            "runner_error": no_result_error,
                        }
                    ]

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

                individual_copy = ""
                individual_copy_error = ""
                if raw_path.exists() and (keep_individual or run_status != "success"):
                    destination = individual_dir / raw_path.name
                    individual_copy_error = copy_if_possible(raw_path, destination)
                    if not individual_copy_error:
                        individual_copy = str(destination)

                run_report = {
                    "run_index": run_index,
                    "repeat": repeat,
                    "status": run_status,
                    "started_at": started_at,
                    "finished_at": now_iso(),
                    "return_code": return_code,
                    "command": command,
                    "parameters": combination,
                    "raw_csv": str(raw_path) if raw_path.exists() else None,
                    "individual_csv": individual_copy or None,
                    "raw_row_count": len(raw_rows),
                    "error": combined_error or None,
                    "csv_write_error": csv_write_error or None,
                    "individual_copy_error": individual_copy_error or None,
                }
                report["runs"].append(run_report)
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

    if report["failed_runs"] or report["salvaged_runs"]:
        return 1
    return 0


def main() -> int:
    if len(sys.argv) != 2:
        print(
            "Usage: python run_resilient.py path/to/experiment.json",
            file=sys.stderr,
        )
        return 2

    spec_path = Path(sys.argv[1]).resolve()
    spec: dict[str, Any] | None = None
    output_dir, output_csv, spec_copy, report_json = result_paths(
        spec_path,
        spec,
    )

    report: dict[str, Any] = {
        "status": "starting",
        "started_at": now_iso(),
        "last_updated_at": now_iso(),
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

    exit_code = 1
    try:
        spec = read_json(spec_path)
        output_dir, output_csv, spec_copy, report_json = result_paths(
            spec_path,
            spec,
        )
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

        copy_error = copy_if_possible(spec_path, spec_copy)
        report["spec_copy_error"] = copy_error or None
        safe_write_json(report_json, report)

        exit_code = run_experiment(
            spec_path=spec_path,
            spec=spec,
            output_dir=output_dir,
            output_csv=output_csv,
            report_json=report_json,
            report=report,
        )

        report["status"] = (
            "completed"
            if exit_code == 0
            else "completed_with_errors"
        )
    except KeyboardInterrupt:
        report["status"] = "interrupted"
        report["fatal_error"] = "Interrupted by user."
        exit_code = 130
    except Exception as error:
        report["status"] = "fatal_error"
        report["fatal_error"] = trim_error(
            "".join(traceback.format_exception(error))
        )
        report["failed_runs"] = int(report.get("failed_runs", 0)) + 1

        if not spec_copy.exists():
            copy_error = copy_if_possible(spec_path, spec_copy)
            report["spec_copy_error"] = copy_error or None

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
                "raw_row_count": 0,
                "error": str(error),
                "csv_write_error": None,
                "individual_copy_error": None,
            }
        )

        try:
            append_rows_flexible(
                output_csv,
                [
                    {
                        "runner_status": "fatal_error",
                        "runner_experiment": (
                            str(spec.get("experiment", spec_path.stem))
                            if isinstance(spec, dict)
                            else spec_path.stem
                        ),
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
        exit_code = 1
    finally:
        report["finished_at"] = now_iso()
        report["last_updated_at"] = report["finished_at"]
        safe_write_json(report_json, report)

        print(f"CSV: {output_csv}")
        print(f"Input JSON copy: {spec_copy}")
        print(f"Run report JSON: {report_json}")
        print(
            "Runs: "
            f"success={report.get('successful_runs', 0)}, "
            f"salvaged={report.get('salvaged_runs', 0)}, "
            f"failed={report.get('failed_runs', 0)}"
        )

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
