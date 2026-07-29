#!/usr/bin/env python3
"""Re-run the campaign's structural and numeric checks without executing TVBPerf."""

from __future__ import annotations

import hashlib
import json
from collections import Counter
from pathlib import Path

import campaign as followup_campaign


EXPERIMENT_DIR = Path(__file__).resolve().parent
RESULTS_DIR = EXPERIMENT_DIR / "results"
MANIFEST_PATH = RESULTS_DIR / "_campaign_manifest.json"
REPORT_PATH = RESULTS_DIR / "_quality_report.json"
campaign = followup_campaign.campaign


def relative(path: Path) -> str:
    return path.resolve().relative_to(campaign.REPO_ROOT.resolve()).as_posix()


def main() -> int:
    manifest = campaign.runner.read_json(MANIFEST_PATH)
    config_reports: list[dict[str, object]] = []
    global_errors: list[str] = []
    consolidated_rows = 0
    csv_statuses: Counter[str] = Counter()

    for entry in manifest["configs"]:
        spec_path = EXPERIMENT_DIR / entry["config"]
        report_path = campaign.REPO_ROOT / entry["report_json"]
        output_csv = campaign.REPO_ROOT / entry["output_csv"]
        run_report = campaign.runner.read_json(report_path)
        errors = campaign.validate_config_outputs(spec_path, run_report)
        fieldnames, rows = campaign.read_csv_rows(output_csv)
        consolidated_rows += len(rows)
        csv_statuses.update(row.get("runner_status", "") for row in rows)
        schema_sha256 = hashlib.sha256(
            "\n".join(fieldnames).encode("utf-8")
        ).hexdigest()
        config_reports.append(
            {
                "config": entry["config"],
                "expected_runs": entry["expected_runs"],
                "consolidated_rows": len(rows),
                "successful_runs": run_report.get("successful_runs", 0),
                "salvaged_runs": run_report.get("salvaged_runs", 0),
                "failed_runs": run_report.get("failed_runs", 0),
                "skipped_runs": run_report.get("skipped_runs", 0),
                "schema_column_count": len(fieldnames),
                "schema_sha256": schema_sha256,
                "errors": errors,
            }
        )
        global_errors.extend(f"{entry['config']}: {error}" for error in errors)

    manifest_accounted = sum(
        int(manifest[name])
        for name in (
            "successful_runs",
            "salvaged_runs",
            "failed_runs",
            "skipped_runs",
        )
    )
    if manifest_accounted != int(manifest["expected_runs"]):
        global_errors.append(
            f"manifest accounted={manifest_accounted} "
            f"expected={manifest['expected_runs']}"
        )
    if consolidated_rows != int(manifest["expected_runs"]):
        global_errors.append(
            f"consolidated rows={consolidated_rows} "
            f"expected={manifest['expected_runs']}"
        )

    backup_paths = sorted(
        relative(path)
        for path in RESULTS_DIR.iterdir()
        if "backup" in path.name.lower()
    )
    if backup_paths:
        global_errors.append(
            "backup data found inside active results: " + ", ".join(backup_paths)
        )

    local_nonmeasurement_paths = sorted(
        relative(path)
        for path in RESULTS_DIR.iterdir()
        if path.name.startswith("_local")
    )
    result = {
        "schema_version": 1,
        "status": "passed" if not global_errors else "failed",
        "campaign_manifest": relative(MANIFEST_PATH),
        "campaign_finished_at": manifest.get("finished_at"),
        "hardware": manifest.get("hardware"),
        "archive_hardware_note": manifest.get("archive_hardware_note"),
        "config_count": len(config_reports),
        "expected_runs": manifest["expected_runs"],
        "consolidated_rows": consolidated_rows,
        "manifest_accounted_runs": manifest_accounted,
        "csv_runner_status_counts": dict(sorted(csv_statuses.items())),
        "duplicate_or_missing_run_errors": sum(
            "duplicate" in error or "missing" in error for error in global_errors
        ),
        "successful_runner_error_violations": sum(
            "runner_error" in error for error in global_errors
        ),
        "backup_paths_in_active_results": backup_paths,
        "excluded_local_nonmeasurement_paths": local_nonmeasurement_paths,
        "global_errors": global_errors,
        "configs": config_reports,
    }
    campaign.write_json_atomic(REPORT_PATH, result)
    print(
        f"quality={result['status']} configs={result['config_count']} "
        f"expected={result['expected_runs']} rows={consolidated_rows} "
        f"errors={len(global_errors)}"
    )
    return 0 if not global_errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
