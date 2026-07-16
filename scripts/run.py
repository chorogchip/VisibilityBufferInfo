# -*- coding: utf-8 -*-

"""Run a benchmark sweep while preserving partial CSV/JSON results.

Usage:
    python run.py path/to/experiment.json

The main execution and recovery policy intentionally stays in this file so it
is easy to review and revise. Reusable details live under ``script_libs``.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
import traceback
from dataclasses import asdict
from pathlib import Path
from typing import Any

from script_libs.common import fail, now_iso, render_argument, trim_error
from script_libs.files import (
    append_rows_flexible,
    copy_if_possible,
    read_result_rows,
    result_paths,
    safe_write_json,
)
from script_libs.models import ProgramArgument
from script_libs.process import command_for, execute
from script_libs.spec import parameter_sets, read_json


def run_experiment(
    spec_path: Path,
    spec: dict[str, Any],
    output_dir: Path,
    output_csv: Path,
    report_json: Path,
    report: dict[str, Any],
) -> int:
    """Execute every parameter set and checkpoint each run immediately."""
    if "output" not in spec:
        fail("Experiment spec requires 'output'.")

    executable = Path(str(spec.get("executable", "../out/build/x64-Release/bin/TVBPerf.exe")))
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

    experiment_name = str(spec.get("experiment", spec_path.stem))
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
                    command, executable.parent, timeout_seconds
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
                    part for part in (process_error, read_error, stderr_text) if part
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
                        f"ERROR: Could not append run {run_index} to {output_csv}: {csv_write_error}",
                        file=sys.stderr,
                    )

                individual_copy = ""
                individual_copy_error = ""
                if raw_path.exists() and (keep_individual or run_status != "success"):
                    destination = individual_dir / raw_path.name
                    individual_copy_error = copy_if_possible(raw_path, destination)
                    if not individual_copy_error:
                        individual_copy = str(destination)

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
                        "individual_csv": individual_copy or None,
                        "raw_row_count": len(raw_rows),
                        "error": combined_error or None,
                        "csv_write_error": csv_write_error or None,
                        "individual_copy_error": individual_copy_error or None,
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
        counter_key = "successful_runs" if run_status == "success" else "salvaged_runs"
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


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: python run.py path/to/experiment.json", file=sys.stderr)
        return 2

    spec_path = Path(sys.argv[1]).resolve()
    spec: dict[str, Any] | None = None
    output_dir, output_csv, spec_copy, report_json = result_paths(spec_path, spec)
    report = initial_report(spec_path, spec_copy, output_csv, report_json)

    exit_code = 1
    try:
        spec = read_json(spec_path)
        output_dir, output_csv, spec_copy, report_json = result_paths(spec_path, spec)
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
            spec_path, spec, output_dir, output_csv, report_json, report
        )
        report["status"] = "completed" if exit_code == 0 else "completed_with_errors"
    except KeyboardInterrupt:
        report["status"] = "interrupted"
        report["fatal_error"] = "Interrupted by user."
        exit_code = 130
    except Exception as error:
        exit_code = handle_fatal_error(
            error, spec, spec_path, spec_copy, output_csv, report
        )
    finally:
        report["finished_at"] = now_iso()
        report["last_updated_at"] = report["finished_at"]
        safe_write_json(report_json, report)
        print_summary(output_csv, spec_copy, report_json, report)

    return exit_code


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
    spec: dict[str, Any] | None,
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


if __name__ == "__main__":
    raise SystemExit(main())
