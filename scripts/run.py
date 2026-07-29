# -*- coding: utf-8 -*-

"""Standalone TVBPerf benchmark sweep runner.

Usage:
    python run.py path/to/experiment.json

This file contains all functionality that previously lived in script_libs.
It checkpoints every run, preserves partial results, and copies per-run CSV
artifacts out of the temporary directory before that directory is deleted.
"""

from __future__ import annotations

import builtins
import csv
import itertools
import json
import math
import re
import shutil
import subprocess
import sys
import tempfile
import traceback
from datetime import datetime
from pathlib import Path
from typing import Any, Iterator


def resilient_print(*args: Any, **kwargs: Any) -> None:
    """Do not let a detached/closed console terminate an active campaign."""
    try:
        builtins.print(*args, **kwargs)
    except (BrokenPipeError, OSError, ValueError):
        pass


# Long-running campaigns can outlive the terminal that started them. Keep all
# existing progress/diagnostic output best-effort while reports remain durable.
print = resilient_print


ERROR_TEXT_LIMIT = 8000
PROGRAM_RESULT_PASS_COUNT = 32
PROGRAM_RESULT_REQUIRED_VALUE_FIELDS = (
    "pass_name_0",
    "pass_0_time_avg_ms",
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
)
PROGRAM_RESULT_REQUIRED_NUMERIC_FIELDS = (
    "pass_0_time_avg_ms",
    "total_time_min_ms",
    "total_time_median_ms",
    "total_time_max_ms",
    "total_time_avg_ms",
    "total_time_p01_ms",
    "total_time_p10_ms",
    "total_time_p90_ms",
    "total_time_p99_ms",
)
PROGRAM_RESULT_STRICTLY_POSITIVE_FIELDS = (
    "total_time_median_ms",
    "total_time_avg_ms",
    "total_time_p90_ms",
    "total_time_p99_ms",
)
UNTRUSTWORTHY_STDERR_PATTERNS = (
    re.compile(r"\bdevice removed\b", re.IGNORECASE),
    re.compile(r"\bDXGI_ERROR_DEVICE_(?:HUNG|REMOVED|RESET)\b", re.IGNORECASE),
    re.compile(r"\bshader (?:compile|compilation) failed\b", re.IGNORECASE),
    re.compile(r"\bPSO (?:creation|initialization) failed\b", re.IGNORECASE),
    re.compile(r"\bassert(?:ion)? failed\b", re.IGNORECASE),
)


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


def argument_enabled(value: Any) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, int | float):
        return value != 0
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "on"}
    return bool(value)


def trim_error(value: Any) -> str:
    text = str(value or "").strip()
    if len(text) <= ERROR_TEXT_LIMIT:
        return text
    return text[-ERROR_TEXT_LIMIT:]


def diagnostic_text(value: Any) -> str:
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace").strip()
    return str(value or "").strip()


def untrustworthy_stderr_error(stderr_text: str) -> str:
    for pattern in UNTRUSTWORTHY_STDERR_PATTERNS:
        match = pattern.search(stderr_text)
        if match:
            return (
                "Renderer emitted an untrustworthy diagnostic: "
                f"{match.group(0)}"
            )
    return ""


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig") as file:
        spec = json.load(file)
    if not isinstance(spec, dict):
        fail("Experiment spec root must be an object.")
    return spec


def normalize_keys(values: dict[str, Any], section: str) -> dict[str, Any]:
    if not isinstance(values, dict):
        fail(f"Experiment spec '{section}' must be an object.")

    return {
        normalized_key: value
        for key, value in values.items()
        if not (normalized_key := key.replace("-", "_")).startswith("_")
    }


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


def copy_directory_if_possible(source: Path, destination: Path) -> str:
    try:
        if not source.exists() or not source.is_dir():
            return f"Directory does not exist: {source}"
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(source, destination)
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
        for row_index, row in enumerate(reader, start=2):
            normalized = {
                str(key): "" if value is None else str(value)
                for key, value in row.items()
                if key is not None
            }
            if not any(value != "" for value in normalized.values()):
                continue

            empty_fields = [
                field_name
                for field_name in PROGRAM_RESULT_REQUIRED_VALUE_FIELDS
                if not normalized.get(field_name, "").strip()
            ]
            if empty_fields:
                fail(
                    f"Benchmark output CSV row {row_index} has empty required "
                    "ProgramResult fields: " + ", ".join(empty_fields)
                )

            numeric_values: dict[str, float] = {}
            for field_name in PROGRAM_RESULT_REQUIRED_NUMERIC_FIELDS:
                try:
                    value = float(normalized[field_name])
                except (TypeError, ValueError) as error:
                    fail(
                        f"Benchmark output CSV row {row_index} has a non-numeric "
                        f"{field_name}: {normalized[field_name]!r} ({error})"
                    )
                if not math.isfinite(value) or value < 0:
                    fail(
                        f"Benchmark output CSV row {row_index} has an invalid "
                        f"{field_name}: {normalized[field_name]!r}"
                    )
                numeric_values[field_name] = value

            non_positive_fields = [
                field_name
                for field_name in PROGRAM_RESULT_STRICTLY_POSITIVE_FIELDS
                if numeric_values[field_name] <= 0
            ]
            if non_positive_fields:
                fail(
                    f"Benchmark output CSV row {row_index} has non-positive "
                    "timing fields: " + ", ".join(non_positive_fields)
                )

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


def command_for(executable: Path, arguments: dict[str, Any]) -> list[str]:
    command = [str(executable)]
    for name, value in arguments.items():
        command.extend(["--" + name.replace("_", "-"), render_argument(value)])
    return command


def execute(
    command: list[str],
    cwd: Path,
    timeout_seconds: float | None,
) -> tuple[int | None, str, str, str, bool]:
    """Return return_code, process_error, stderr_text, failure_kind, interrupted."""
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
        stderr_text = diagnostic_text(completed.stderr)
        process_error = (
            ""
            if completed.returncode == 0
            else f"Process exited with code {completed.returncode}."
        )
        failure_kind = "" if completed.returncode == 0 else "nonzero_exit"
        return (
            completed.returncode,
            process_error,
            stderr_text,
            failure_kind,
            False,
        )
    except subprocess.TimeoutExpired as error:
        return (
            None,
            f"Process timed out after {timeout_seconds} second(s).",
            diagnostic_text(error.stderr),
            "timeout",
            False,
        )
    except KeyboardInterrupt:
        return None, "Interrupted by user.", "", "interrupted", True
    except Exception as error:
        return (
            None,
            f"Could not execute benchmark: {error}",
            "",
            "start_failure",
            False,
        )


def classify_run(
    *,
    raw_rows: list[dict[str, str]],
    return_code: int | None,
    process_error: str,
    read_error: str,
    stderr_text: str,
    failure_kind: str,
    raw_path: Path,
) -> tuple[str, str]:
    """Classify a run without treating ordinary stderr diagnostics as errors."""
    trust_error = untrustworthy_stderr_error(stderr_text)
    runner_error = " | ".join(
        part for part in (process_error, read_error, trust_error) if part
    )

    if failure_kind in {"timeout", "start_failure", "interrupted"}:
        return "failed", runner_error
    if trust_error or read_error:
        return "failed", runner_error
    if return_code == 0 and raw_rows:
        return "success", ""
    if failure_kind == "nonzero_exit" and raw_rows:
        return "salvaged", runner_error

    no_result_error = runner_error or (
        f"Benchmark output CSV was not created or had no valid rows: {raw_path}"
    )
    return "failed", no_result_error


def build_csv_rows(
    *,
    raw_rows: list[dict[str, str]],
    combination: dict[str, Any],
    experiment_name: str,
    repeat: int,
    run_index: int,
    return_code: int | None,
    run_status: str,
    runner_error: str,
    stderr_text: str,
    report: dict[str, Any],
) -> list[dict[str, Any]]:
    """Create consolidated rows and increment exactly one run-status counter."""
    parameter_columns = {
        f"param_{name}": render_argument(value)
        for name, value in combination.items()
    }

    counter_keys = {
        "success": "successful_runs",
        "salvaged": "salvaged_runs",
        "failed": "failed_runs",
    }
    report[counter_keys[run_status]] += 1
    result_rows: list[dict[str, str] | dict[str, Any]] = raw_rows or [{}]
    return [
        {
            **result_row,
            **parameter_columns,
            "runner_experiment": experiment_name,
            "runner_repeat": repeat,
            "runner_run_index": run_index,
            "runner_result_row": result_row_index if raw_rows else "",
            "runner_status": run_status,
            "runner_return_code": "" if return_code is None else return_code,
            "runner_error": runner_error,
            "runner_stderr": stderr_text,
            "runner_skip_reason": "",
            "runner_missing_assets": "",
        }
        for result_row_index, result_row in enumerate(result_rows)
    ]


def build_skipped_csv_row(
    *,
    combination: dict[str, Any],
    experiment_name: str,
    repeat: int,
    run_index: int,
    missing_assets: list[dict[str, str]],
    report: dict[str, Any],
) -> dict[str, Any]:
    parameter_columns = {
        f"param_{name}": render_argument(value)
        for name, value in combination.items()
    }
    missing_paths = [asset["resolved_path"] for asset in missing_assets]
    skip_reason = "Missing required asset(s): " + "; ".join(missing_paths)
    report["skipped_runs"] += 1
    return {
        **parameter_columns,
        "runner_experiment": experiment_name,
        "runner_repeat": repeat,
        "runner_run_index": run_index,
        "runner_result_row": "",
        "runner_status": "skipped_missing_asset",
        "runner_return_code": "",
        "runner_error": "",
        "runner_stderr": "",
        "runner_skip_reason": skip_reason,
        "runner_missing_assets": ";".join(missing_paths),
    }


def resolve_runtime_path(value: Any, runtime_dir: Path) -> Path:
    path = Path(str(value))
    if path.is_absolute():
        return path.resolve()
    return (runtime_dir / path).resolve()


def missing_combination_assets(
    combination: dict[str, Any],
    runtime_dir: Path,
) -> list[dict[str, str]]:
    required: list[tuple[str, Any]] = []
    if argument_enabled(combination.get("to_use_scene", False)):
        required.append(("scene_path", combination.get("scene_path", "")))

    try:
        camera_mode = int(combination.get("camera_mode", 0))
    except (TypeError, ValueError):
        camera_mode = -1
    if camera_mode in {1, 2}:
        required.append(
            ("camera_filepath", combination.get("camera_filepath", ""))
        )

    missing: list[dict[str, str]] = []
    for argument_name, raw_value in required:
        if not str(raw_value).strip():
            resolved = runtime_dir / "<empty>"
        else:
            resolved = resolve_runtime_path(raw_value, runtime_dir)
        if not resolved.exists() or not resolved.is_file():
            missing.append(
                {
                    "argument": argument_name,
                    "configured_path": str(raw_value),
                    "resolved_path": str(resolved),
                }
            )
    return missing


def validate_runtime_files(executable: Path) -> None:
    runtime_dir = executable.parent
    required_files = (
        runtime_dir / "dxcompiler.dll",
        runtime_dir / "dxil.dll",
    )
    missing = [
        str(path)
        for path in required_files
        if not path.exists() or not path.is_file()
    ]
    shader_dir = runtime_dir / "assets" / "shaders"
    if not shader_dir.exists() or not shader_dir.is_dir():
        missing.append(str(shader_dir))
    if missing:
        fail("TVBPerf runtime dependency is missing: " + "; ".join(missing))


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
            "skipped_runs": 0,
        }
    )
    safe_write_json(report_json, report)

    if not executable.exists() or not executable.is_file():
        fail(f"TVBPerf executable does not exist or is not a file: {executable}")
    validate_runtime_files(executable)

    individual_dir = output_dir / f"{output_csv.stem}_runs"
    temporary_dir = Path(tempfile.mkdtemp(prefix="tvbperf_"))
    run_index = 0

    try:
        for repeat in range(repeat_count):
            for combination in combinations:
                raw_path = temporary_dir / f"run_{run_index:05d}.csv"
                started_at = now_iso()
                missing_assets = missing_combination_assets(
                    combination,
                    executable.parent,
                )
                if missing_assets:
                    csv_row = build_skipped_csv_row(
                        combination=combination,
                        experiment_name=experiment_name,
                        repeat=repeat,
                        run_index=run_index,
                        missing_assets=missing_assets,
                        report=report,
                    )
                    csv_write_error = ""
                    try:
                        append_rows_flexible(output_csv, [csv_row])
                    except Exception as error:
                        csv_write_error = trim_error(error)
                        print(
                            f"ERROR: Could not append skipped run {run_index} "
                            f"to {output_csv}: {csv_write_error}",
                            file=sys.stderr,
                        )

                    missing_paths = [
                        asset["resolved_path"] for asset in missing_assets
                    ]
                    skip_reason = (
                        "Missing required asset(s): " + "; ".join(missing_paths)
                    )
                    report["runs"].append(
                        {
                            "run_index": run_index,
                            "repeat": repeat,
                            "status": "skipped_missing_asset",
                            "started_at": started_at,
                            "finished_at": now_iso(),
                            "return_code": None,
                            "command": None,
                            "parameters": combination,
                            "raw_csv": None,
                            "individual_csv": None,
                            "artifact_csvs": [],
                            "artifact_dirs": [],
                            "capture_output_dir": None,
                            "raw_row_count": 0,
                            "error": None,
                            "stderr": None,
                            "failure_kind": None,
                            "skip_reason": skip_reason,
                            "missing_assets": missing_assets,
                            "csv_write_error": csv_write_error or None,
                            "individual_copy_error": None,
                            "artifact_copy_errors": [],
                        }
                    )
                    report["completed_runs"] = len(report["runs"])
                    report["last_updated_at"] = now_iso()
                    safe_write_json(report_json, report)
                    print(
                        f"[{run_index + 1}/{total}] "
                        f"skipped_missing_asset: {skip_reason}"
                    )
                    run_index += 1
                    continue

                capture_requested = argument_enabled(
                    combination.get("capture_frames", False)
                )
                raw_capture_dir = (
                    temporary_dir / f"run_{run_index:05d}_capture"
                    if capture_requested
                    else None
                )
                arguments = {
                    **combination,
                    "run_id": run_index,
                    "run_name": experiment_name,
                    "output_filepath": str(raw_path),
                    "auto_terminate": True,
                }
                if raw_capture_dir is not None:
                    arguments["capture_output_dir"] = str(raw_capture_dir)

                command = command_for(executable, arguments)
                print(f"[{run_index + 1}/{total}] {subprocess.list2cmdline(command)}")

                (
                    return_code,
                    process_error,
                    stderr_text,
                    failure_kind,
                    interrupted,
                ) = execute(command, executable.parent, timeout_seconds)
                if stderr_text:
                    print(stderr_text, file=sys.stderr)

                read_error = ""
                try:
                    raw_rows = read_result_rows(raw_path)
                except Exception as error:
                    raw_rows = []
                    read_error = f"Could not read benchmark CSV: {error}"

                run_status, runner_error = classify_run(
                    raw_rows=raw_rows,
                    return_code=return_code,
                    process_error=process_error,
                    read_error=read_error,
                    stderr_text=stderr_text,
                    failure_kind=failure_kind,
                    raw_path=raw_path,
                )
                csv_rows = build_csv_rows(
                    raw_rows=raw_rows,
                    combination=combination,
                    experiment_name=experiment_name,
                    repeat=repeat,
                    run_index=run_index,
                    return_code=return_code,
                    run_status=run_status,
                    runner_error=runner_error,
                    stderr_text=stderr_text,
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
                artifact_dirs: list[str] = []

                if raw_capture_dir is not None:
                    destination = individual_dir / raw_capture_dir.name
                    copy_error = copy_directory_if_possible(
                        raw_capture_dir,
                        destination,
                    )
                    if copy_error:
                        artifact_copy_errors.append(
                            f"{raw_capture_dir.name}: {copy_error}"
                        )
                    else:
                        artifact_dirs.append(str(destination))

                if artifact_csvs:
                    print("  -> copied sidecar CSV(s):")
                    for artifact_path in artifact_csvs:
                        print(f"     {artifact_path}")
                if artifact_dirs:
                    print("  -> copied artifact directories:")
                    for artifact_dir in artifact_dirs:
                        print(f"     {artifact_dir}")
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
                        "artifact_dirs": artifact_dirs,
                        "capture_output_dir": (
                            str(raw_capture_dir) if raw_capture_dir is not None else None
                        ),
                        "raw_row_count": len(raw_rows),
                        "error": runner_error or None,
                        "stderr": stderr_text or None,
                        "failure_kind": failure_kind or None,
                        "skip_reason": None,
                        "missing_assets": [],
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
                    + (f"; {runner_error}" if runner_error else "")
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
        "skipped_runs": 0,
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
            "artifact_dirs": [],
            "capture_output_dir": None,
            "raw_row_count": 0,
            "error": str(error),
            "stderr": None,
            "failure_kind": "fatal_error",
            "skip_reason": None,
            "missing_assets": [],
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
                    "runner_stderr": "",
                    "runner_skip_reason": "",
                    "runner_missing_assets": "",
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
        f"failed={report.get('failed_runs', 0)}, "
        f"skipped={report.get('skipped_runs', 0)}"
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
        if exit_code != 0:
            report["status"] = "completed_with_errors"
        elif report.get("skipped_runs", 0):
            report["status"] = "completed_with_skips"
        else:
            report["status"] = "completed"
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

