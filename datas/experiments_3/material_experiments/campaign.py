#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Run and validate material experiment JSON files one at a time.

The campaign manifest locks the top-level runnable JSON list and checkpoints
every config.  Invoke ``run`` separately for each config; completed configs are
not rerun unless ``--force`` is supplied.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Any


EXPERIMENT_DIR = Path(__file__).resolve().parent
SCRIPTS_DIR = EXPERIMENT_DIR.parent
REPO_ROOT = SCRIPTS_DIR.parent
RESULTS_DIR = EXPERIMENT_DIR / "results"
MANIFEST_PATH = RESULTS_DIR / "_campaign_manifest.json"

sys.path.insert(0, str(SCRIPTS_DIR))
import run as runner  # noqa: E402


TERMINAL_CONFIG_STATUSES = {"completed", "completed_with_skips"}
RUNNER_STATUS_TO_COUNTER = {
    "success": "successful_runs",
    "salvaged": "salvaged_runs",
    "failed": "failed_runs",
    "skipped_missing_asset": "skipped_runs",
}


def now_iso() -> str:
    return datetime.now().astimezone().isoformat(timespec="seconds")


def relative_to_repo(path: Path) -> str:
    try:
        return path.resolve().relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return str(path.resolve())


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = path.with_suffix(path.suffix + ".tmp")
    with temporary_path.open("w", encoding="utf-8", newline="\n") as file:
        json.dump(value, file, ensure_ascii=False, indent=2)
        file.write("\n")
    temporary_path.replace(path)


def read_manifest() -> dict[str, Any]:
    if not MANIFEST_PATH.exists():
        raise FileNotFoundError(
            f"Campaign manifest does not exist; run init first: {MANIFEST_PATH}"
        )
    return runner.read_json(MANIFEST_PATH)


def discover_runnable_configs() -> list[Path]:
    configs: list[Path] = []
    for path in sorted(EXPERIMENT_DIR.glob("*.json"), key=lambda item: item.name):
        spec = runner.read_json(path)
        if spec.get("_status") == "runnable_now":
            configs.append(path)
    return configs


def expected_run_count(spec: dict[str, Any]) -> int:
    combinations, _ = runner.parameter_sets(spec)
    repeat = int(spec.get("repeat", 1))
    if repeat < 1:
        raise ValueError("'repeat' must be at least 1.")
    return len(combinations) * repeat


def resolve_executable(spec_path: Path, spec: dict[str, Any]) -> Path:
    executable = Path(
        str(spec.get("executable", "../out/build/x64-Release/bin/TVBPerf.exe"))
    )
    if not executable.is_absolute():
        executable = (spec_path.parent / executable).resolve()
    return executable


def validate_synthetic_base_camera(
    spec_path: Path,
    spec: dict[str, Any],
) -> list[str]:
    base = runner.normalize_keys(spec.get("base", {}), "base")
    if runner.argument_enabled(base.get("to_use_scene", False)):
        return []

    intended = {
        "camera_pos_z": -3.0,
        "camera_lookat_x": 0.0,
        "camera_lookat_y": 0.0,
        "camera_lookat_z": 0.0,
        "z_min": -1.0,
        "z_max": 1.0,
        "xy_minmax": 1.0,
    }
    errors: list[str] = []
    for name, expected in intended.items():
        try:
            actual = float(base[name])
        except (KeyError, TypeError, ValueError):
            errors.append(f"{spec_path.name}: synthetic base has invalid {name}")
            continue
        if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=1e-6):
            errors.append(
                f"{spec_path.name}: synthetic base {name}={actual}, "
                f"expected {expected}"
            )
    return errors


def preflight_config(spec_path: Path) -> dict[str, Any]:
    spec = runner.read_json(spec_path)
    if spec.get("_status") != "runnable_now":
        raise ValueError(
            f"{spec_path.name} is not runnable_now: {spec.get('_status')!r}"
        )

    executable = resolve_executable(spec_path, spec)
    runtime_errors: list[str] = []
    if not executable.exists() or not executable.is_file():
        runtime_errors.append(f"Missing executable: {executable}")
    else:
        try:
            runner.validate_runtime_files(executable)
        except Exception as error:
            runtime_errors.append(str(error))

    combinations, parameter_source = runner.parameter_sets(spec)
    repeat = int(spec.get("repeat", 1))
    missing_runs: list[dict[str, Any]] = []
    if executable.exists():
        for combination_index, combination in enumerate(combinations):
            missing_assets = runner.missing_combination_assets(
                combination,
                executable.parent,
            )
            if missing_assets:
                for repeat_index in range(repeat):
                    missing_runs.append(
                        {
                            "combination_index": combination_index,
                            "repeat": repeat_index,
                            "missing_assets": missing_assets,
                        }
                    )

    absolute_argument_paths: list[dict[str, Any]] = []
    for combination_index, combination in enumerate(combinations):
        for name in ("scene_path", "camera_filepath"):
            value = combination.get(name)
            if value and Path(str(value)).is_absolute():
                absolute_argument_paths.append(
                    {
                        "combination_index": combination_index,
                        "argument": name,
                        "path": str(value),
                    }
                )

    synthetic_camera_errors = validate_synthetic_base_camera(spec_path, spec)
    return {
        "checked_at": now_iso(),
        "config": spec_path.name,
        "parameter_source": parameter_source,
        "parameter_set_count": len(combinations),
        "repeat": repeat,
        "expected_runs": len(combinations) * repeat,
        "runnable_runs": len(combinations) * repeat - len(missing_runs),
        "missing_asset_runs": len(missing_runs),
        "missing_runs": missing_runs,
        "runtime_errors": runtime_errors,
        "absolute_argument_paths": absolute_argument_paths,
        "synthetic_camera_errors": synthetic_camera_errors,
        "executable": relative_to_repo(executable),
    }


def manifest_entry(spec_path: Path) -> dict[str, Any]:
    spec = runner.read_json(spec_path)
    _, output_csv, _, report_json = runner.result_paths(spec_path)
    return {
        "config": spec_path.name,
        "started_at": None,
        "finished_at": None,
        "expected_runs": expected_run_count(spec),
        "successful_runs": 0,
        "salvaged_runs": 0,
        "failed_runs": 0,
        "skipped_runs": 0,
        "status": "pending",
        "error_summary": [],
        "output_csv": relative_to_repo(output_csv),
        "report_json": relative_to_repo(report_json),
        "runner_exit_code": None,
        "preflight": None,
        "validation_errors": [],
    }


def initialize_campaign() -> dict[str, Any]:
    if MANIFEST_PATH.exists():
        manifest = read_manifest()
        print(f"Resuming existing campaign: {MANIFEST_PATH}")
        return manifest

    backup_path: Path | None = None
    if RESULTS_DIR.exists():
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_path = EXPERIMENT_DIR / f"results_backup_{stamp}"
        if backup_path.exists():
            raise FileExistsError(f"Results backup already exists: {backup_path}")
        RESULTS_DIR.replace(backup_path)

    RESULTS_DIR.mkdir(parents=True, exist_ok=False)
    configs = discover_runnable_configs()
    ivy_path = (
        REPO_ROOT
        / "assets"
        / "scenes"
        / "unpacked"
        / "main_sponza_ivy"
        / "NewSponza_Main_Ivy_glTF.gltf"
    )
    manifest = {
        "schema_version": 1,
        "campaign_status": "running",
        "started_at": now_iso(),
        "finished_at": None,
        "last_updated_at": now_iso(),
        "repository_root": str(REPO_ROOT),
        "results_dir": relative_to_repo(RESULTS_DIR),
        "results_backup": (
            relative_to_repo(backup_path) if backup_path is not None else None
        ),
        "locked_runnable_configs": [path.name for path in configs],
        "config_count": len(configs),
        "expected_runs": sum(
            expected_run_count(runner.read_json(path)) for path in configs
        ),
        "successful_runs": 0,
        "salvaged_runs": 0,
        "failed_runs": 0,
        "skipped_runs": 0,
        "ivy": {
            "exists": ivy_path.exists() and ivy_path.is_file(),
            "path": relative_to_repo(ivy_path),
        },
        "configs": [manifest_entry(path) for path in configs],
    }
    write_json_atomic(MANIFEST_PATH, manifest)
    print(
        f"Initialized {len(configs)} configs, "
        f"{manifest['expected_runs']} expected runs."
    )
    return manifest


def find_entry(
    manifest: dict[str, Any],
    config_name: str,
) -> dict[str, Any]:
    for entry in manifest["configs"]:
        if entry["config"] == config_name:
            return entry
    raise KeyError(f"Config is not in the locked campaign: {config_name}")


def remove_config_results(spec_path: Path) -> None:
    output_dir, _, _, _ = runner.result_paths(spec_path)
    resolved = output_dir.resolve()
    if resolved.parent != RESULTS_DIR.resolve():
        raise ValueError(f"Refusing to remove unexpected result path: {resolved}")
    if resolved.exists():
        shutil.rmtree(resolved)


def read_csv_rows(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open("r", encoding="utf-8-sig", newline="") as file:
        csv_reader = csv.DictReader(file)
        if not csv_reader.fieldnames:
            raise ValueError(f"CSV has no header: {path}")
        return list(csv_reader.fieldnames), [dict(row) for row in csv_reader]


def validate_config_outputs(
    spec_path: Path,
    report: dict[str, Any],
) -> list[str]:
    errors: list[str] = []
    spec = runner.read_json(spec_path)
    expected = expected_run_count(spec)
    output_dir, output_csv, _, _ = runner.result_paths(spec_path)

    report_counts = {
        name: int(report.get(name, 0))
        for name in (
            "successful_runs",
            "salvaged_runs",
            "failed_runs",
            "skipped_runs",
        )
    }
    if int(report.get("total_runs", -1)) != expected:
        errors.append(
            f"report total_runs={report.get('total_runs')} != expected={expected}"
        )
    if sum(report_counts.values()) != expected:
        errors.append(
            f"report status-count sum={sum(report_counts.values())} "
            f"!= expected={expected}"
        )
    if int(report.get("completed_runs", -1)) != expected:
        errors.append(
            f"report completed_runs={report.get('completed_runs')} "
            f"!= expected={expected}"
        )

    report_runs = report.get("runs", [])
    report_indexes = [int(run["run_index"]) for run in report_runs]
    if report_indexes != list(range(expected)):
        errors.append("report run indexes are duplicated, missing, or out of order")

    if not output_csv.exists() or output_csv.stat().st_size == 0:
        errors.append(f"output CSV is missing or empty: {output_csv}")
        return errors

    try:
        fieldnames, rows = read_csv_rows(output_csv)
    except Exception as error:
        errors.append(f"could not read output CSV: {error}")
        return errors

    required_runner_fields = {
        "runner_experiment",
        "runner_repeat",
        "runner_run_index",
        "runner_result_row",
        "runner_status",
        "runner_return_code",
        "runner_error",
        "runner_stderr",
        "runner_skip_reason",
        "runner_missing_assets",
    }
    missing_runner_fields = sorted(required_runner_fields - set(fieldnames))
    if missing_runner_fields:
        errors.append(
            "output CSV is missing runner fields: "
            + ", ".join(missing_runner_fields)
        )

    combinations, _ = runner.parameter_sets(spec)
    repeat_count = int(spec.get("repeat", 1))
    expected_parameters = [
        (repeat, combination)
        for repeat in range(repeat_count)
        for combination in combinations
    ]
    expected_parameter_columns = {
        f"param_{name}"
        for _, combination in expected_parameters
        for name in combination
    }
    missing_parameter_columns = sorted(
        expected_parameter_columns - set(fieldnames)
    )
    if missing_parameter_columns:
        errors.append(
            "output CSV is missing parameter fields: "
            + ", ".join(missing_parameter_columns)
        )

    rows_by_index: dict[int, list[dict[str, str]]] = {}
    row_keys: set[tuple[str, str, str]] = set()
    for row_number, row in enumerate(rows, start=2):
        try:
            run_index = int(row.get("runner_run_index", ""))
        except ValueError:
            errors.append(f"CSV row {row_number} has invalid runner_run_index")
            continue
        rows_by_index.setdefault(run_index, []).append(row)
        row_key = (
            row.get("runner_repeat", ""),
            row.get("runner_run_index", ""),
            row.get("runner_result_row", ""),
        )
        if row_key in row_keys:
            errors.append(f"CSV has duplicate runner row key: {row_key}")
        row_keys.add(row_key)

    if sorted(rows_by_index) != list(range(expected)):
        errors.append("CSV run indexes are duplicated, missing, or out of range")

    parameter_keys: set[tuple[int, str]] = set()
    for run_index, run_report in enumerate(report_runs):
        run_rows = rows_by_index.get(run_index, [])
        if not run_rows:
            continue
        status = str(run_report.get("status", ""))
        if status not in RUNNER_STATUS_TO_COUNTER:
            errors.append(f"run {run_index} has unknown status: {status}")
            continue

        for row in run_rows:
            if row.get("runner_status") != status:
                errors.append(
                    f"run {run_index} report/CSV status mismatch: "
                    f"{status!r} vs {row.get('runner_status')!r}"
                )

        expected_repeat, parameters = expected_parameters[run_index]
        parameter_fingerprint = json.dumps(
            parameters,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        )
        duplicate_key = (expected_repeat, parameter_fingerprint)
        if duplicate_key in parameter_keys:
            errors.append(
                f"run {run_index} unintentionally duplicates an earlier condition"
            )
        parameter_keys.add(duplicate_key)

        for row in run_rows:
            if row.get("runner_repeat") != str(expected_repeat):
                errors.append(f"run {run_index} has the wrong repeat index")
            for name, value in parameters.items():
                column = f"param_{name}"
                if row.get(column, "") != runner.render_argument(value):
                    errors.append(
                        f"run {run_index} does not preserve parameter {name}"
                    )

        if status in {"success", "salvaged"}:
            for row in run_rows:
                if status == "success" and row.get("runner_error", "").strip():
                    errors.append(
                        f"successful run {run_index} has runner_error text"
                    )
                if row.get("runner_error", "").strip() == "Log saved to:":
                    errors.append(
                        f"run {run_index} misclassified a log path as runner_error"
                    )
                for field_name in runner.PROGRAM_RESULT_REQUIRED_VALUE_FIELDS:
                    if not row.get(field_name, "").strip():
                        errors.append(
                            f"run {run_index} has empty {field_name}"
                        )
                for field_name in runner.PROGRAM_RESULT_REQUIRED_NUMERIC_FIELDS:
                    try:
                        value = float(row.get(field_name, ""))
                    except ValueError:
                        errors.append(
                            f"run {run_index} has non-numeric {field_name}"
                        )
                        continue
                    if not math.isfinite(value) or value < 0:
                        errors.append(
                            f"run {run_index} has invalid {field_name}={value}"
                        )
                nonempty_passes = 0
                for pass_index in range(runner.PROGRAM_RESULT_PASS_COUNT):
                    pass_name = row.get(f"pass_name_{pass_index}", "").strip()
                    pass_time = row.get(
                        f"pass_{pass_index}_time_avg_ms", ""
                    ).strip()
                    if not pass_name and not pass_time:
                        continue
                    nonempty_passes += 1
                    try:
                        value = float(pass_time)
                    except ValueError:
                        errors.append(
                            f"run {run_index} has non-numeric pass "
                            f"{pass_index} timing"
                        )
                        continue
                    if not math.isfinite(value) or value < 0:
                        errors.append(
                            f"run {run_index} has invalid pass "
                            f"{pass_index} timing"
                        )
                if nonempty_passes == 0:
                    errors.append(f"run {run_index} has no pass timing data")
        elif status == "skipped_missing_asset":
            for row in run_rows:
                if not row.get("runner_skip_reason", "").strip():
                    errors.append(f"skipped run {run_index} has no skip reason")
                if not row.get("runner_missing_assets", "").strip():
                    errors.append(
                        f"skipped run {run_index} has no missing-asset path"
                    )

    if output_dir.parent.resolve() != RESULTS_DIR.resolve():
        errors.append("result path escaped the campaign results directory")
    return sorted(set(errors))


def update_campaign_totals(manifest: dict[str, Any]) -> None:
    for counter in (
        "successful_runs",
        "salvaged_runs",
        "failed_runs",
        "skipped_runs",
    ):
        manifest[counter] = sum(
            int(entry.get(counter, 0)) for entry in manifest["configs"]
        )
    manifest["last_updated_at"] = now_iso()


def run_single_config(config_name: str, force: bool) -> int:
    manifest = read_manifest()
    entry = find_entry(manifest, config_name)
    spec_path = EXPERIMENT_DIR / config_name

    if entry["status"] in TERMINAL_CONFIG_STATUSES and not force:
        print(
            f"{config_name} is already {entry['status']}; "
            "leaving its results unchanged."
        )
        return 0

    if force or entry["status"] in {"failed", "running"}:
        remove_config_results(spec_path)

    preflight = preflight_config(spec_path)
    entry.update(
        {
            "started_at": now_iso(),
            "finished_at": None,
            "successful_runs": 0,
            "salvaged_runs": 0,
            "failed_runs": 0,
            "skipped_runs": 0,
            "status": "running",
            "error_summary": [],
            "runner_exit_code": None,
            "preflight": preflight,
            "validation_errors": [],
        }
    )
    update_campaign_totals(manifest)
    write_json_atomic(MANIFEST_PATH, manifest)

    if preflight["runtime_errors"] or preflight["absolute_argument_paths"]:
        entry["status"] = "failed"
        entry["finished_at"] = now_iso()
        entry["error_summary"] = (
            preflight["runtime_errors"]
            + [
                "Absolute scene/camera paths are not reproducible: "
                + json.dumps(
                    preflight["absolute_argument_paths"],
                    ensure_ascii=False,
                )
            ]
        )
        update_campaign_totals(manifest)
        write_json_atomic(MANIFEST_PATH, manifest)
        return 1

    command = [sys.executable, str(SCRIPTS_DIR / "run.py"), str(spec_path)]
    print("Executing one config:", subprocess.list2cmdline(command))
    completed = subprocess.run(command, cwd=REPO_ROOT, check=False)
    entry["runner_exit_code"] = completed.returncode

    _, _, _, report_path = runner.result_paths(spec_path)
    error_summary: list[str] = []
    if not report_path.exists():
        report: dict[str, Any] = {}
        error_summary.append(f"Run report was not created: {report_path}")
    else:
        report = runner.read_json(report_path)
        for counter in (
            "successful_runs",
            "salvaged_runs",
            "failed_runs",
            "skipped_runs",
        ):
            entry[counter] = int(report.get(counter, 0))

    validation_errors = (
        validate_config_outputs(spec_path, report) if report else []
    )
    entry["validation_errors"] = validation_errors
    error_summary.extend(validation_errors)

    if completed.returncode != 0:
        error_summary.append(f"runner exit code: {completed.returncode}")
    if entry["salvaged_runs"]:
        error_summary.append(
            f"salvaged runs require investigation: {entry['salvaged_runs']}"
        )
    if entry["failed_runs"]:
        error_summary.append(f"failed runs: {entry['failed_runs']}")

    entry["finished_at"] = now_iso()
    entry["error_summary"] = sorted(set(error_summary))
    if error_summary:
        entry["status"] = "failed"
    elif entry["skipped_runs"]:
        entry["status"] = "completed_with_skips"
    else:
        entry["status"] = "completed"

    update_campaign_totals(manifest)
    write_json_atomic(MANIFEST_PATH, manifest)
    print(
        f"{config_name}: status={entry['status']} "
        f"expected={entry['expected_runs']} "
        f"success={entry['successful_runs']} "
        f"salvaged={entry['salvaged_runs']} "
        f"failed={entry['failed_runs']} "
        f"skipped={entry['skipped_runs']}"
    )
    return 0 if entry["status"] in TERMINAL_CONFIG_STATUSES else 1


def preflight_command(config_name: str | None) -> int:
    configs = (
        [EXPERIMENT_DIR / config_name]
        if config_name
        else discover_runnable_configs()
    )
    failed = False
    for spec_path in configs:
        result = preflight_config(spec_path)
        print(
            f"{spec_path.name}: expected={result['expected_runs']} "
            f"runnable={result['runnable_runs']} "
            f"missing_asset={result['missing_asset_runs']}"
        )
        for key in (
            "runtime_errors",
            "absolute_argument_paths",
            "synthetic_camera_errors",
        ):
            for value in result[key]:
                print(f"  {key}: {value}")
                if key != "synthetic_camera_errors":
                    failed = True
        for missing_run in result["missing_runs"]:
            print(
                "  skipped_missing_asset candidate: "
                + json.dumps(missing_run, ensure_ascii=False)
            )
    return 1 if failed else 0


def finalize_campaign() -> int:
    manifest = read_manifest()
    errors: list[str] = []
    for entry in manifest["configs"]:
        if entry["status"] not in TERMINAL_CONFIG_STATUSES:
            errors.append(f"{entry['config']}: {entry['status']}")
    update_campaign_totals(manifest)
    accounted = (
        manifest["successful_runs"]
        + manifest["salvaged_runs"]
        + manifest["failed_runs"]
        + manifest["skipped_runs"]
    )
    if accounted != manifest["expected_runs"]:
        errors.append(
            f"accounted runs {accounted} != expected {manifest['expected_runs']}"
        )
    manifest["campaign_status"] = "completed" if not errors else "failed"
    manifest["finished_at"] = now_iso()
    manifest["final_errors"] = errors
    write_json_atomic(MANIFEST_PATH, manifest)
    print(
        f"campaign={manifest['campaign_status']} "
        f"expected={manifest['expected_runs']} "
        f"success={manifest['successful_runs']} "
        f"salvaged={manifest['salvaged_runs']} "
        f"failed={manifest['failed_runs']} "
        f"skipped={manifest['skipped_runs']}"
    )
    for error in errors:
        print("  " + error)
    return 1 if errors else 0


def print_status() -> int:
    manifest = read_manifest()
    for entry in manifest["configs"]:
        print(
            f"{entry['config']}: {entry['status']} "
            f"{entry['successful_runs']}/{entry['expected_runs']} success, "
            f"{entry['salvaged_runs']} salvaged, "
            f"{entry['failed_runs']} failed, "
            f"{entry['skipped_runs']} skipped"
        )
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("init")
    preflight_parser = subparsers.add_parser("preflight")
    preflight_parser.add_argument("config", nargs="?")
    run_parser = subparsers.add_parser("run")
    run_parser.add_argument("config")
    run_parser.add_argument("--force", action="store_true")
    subparsers.add_parser("status")
    subparsers.add_parser("finalize")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.command == "init":
        initialize_campaign()
        return 0
    if args.command == "preflight":
        return preflight_command(args.config)
    if args.command == "run":
        return run_single_config(args.config, args.force)
    if args.command == "status":
        return print_status()
    if args.command == "finalize":
        return finalize_campaign()
    raise AssertionError(args.command)


if __name__ == "__main__":
    raise SystemExit(main())
