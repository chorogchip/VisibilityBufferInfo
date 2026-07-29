#!/usr/bin/env python3
"""Run the post-fairness campaigns one JSON at a time.

This reuses the material campaign's preflight, runner, output validation, and
resume logic.  Initialization also imports already completed smoke results
instead of deleting or duplicating them.
"""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path
from typing import Any


EXPERIMENT_DIR = Path(__file__).resolve().parent
BASE_CAMPAIGN_PATH = EXPERIMENT_DIR.parent / "material_experiments" / "campaign.py"

module_spec = importlib.util.spec_from_file_location(
    "material_campaign_base",
    BASE_CAMPAIGN_PATH,
)
if module_spec is None or module_spec.loader is None:
    raise RuntimeError(f"Could not load campaign support: {BASE_CAMPAIGN_PATH}")

campaign = importlib.util.module_from_spec(module_spec)
sys.modules[module_spec.name] = campaign
module_spec.loader.exec_module(campaign)

campaign.EXPERIMENT_DIR = EXPERIMENT_DIR
campaign.RESULTS_DIR = EXPERIMENT_DIR / "results"
campaign.MANIFEST_PATH = campaign.RESULTS_DIR / "_campaign_manifest.json"


def import_existing_result(
    spec_path: Path,
    entry: dict[str, Any],
) -> None:
    _, _, _, report_path = campaign.runner.result_paths(spec_path)
    if not report_path.exists():
        return

    report = campaign.runner.read_json(report_path)
    errors = campaign.validate_config_outputs(spec_path, report)
    for counter in (
        "successful_runs",
        "salvaged_runs",
        "failed_runs",
        "skipped_runs",
    ):
        entry[counter] = int(report.get(counter, 0))

    entry["started_at"] = report.get("started_at")
    entry["finished_at"] = report.get("finished_at")
    entry["runner_exit_code"] = 0
    entry["validation_errors"] = errors
    entry["error_summary"] = list(errors)
    if entry["salvaged_runs"]:
        entry["error_summary"].append(
            f"salvaged runs require investigation: {entry['salvaged_runs']}"
        )
    if entry["failed_runs"]:
        entry["error_summary"].append(f"failed runs: {entry['failed_runs']}")

    if entry["error_summary"]:
        entry["status"] = "failed"
    elif entry["skipped_runs"]:
        entry["status"] = "completed_with_skips"
    else:
        entry["status"] = "completed"


def initialize_campaign() -> dict[str, Any]:
    if campaign.MANIFEST_PATH.exists():
        manifest = campaign.read_manifest()
        print(f"Resuming existing campaign: {campaign.MANIFEST_PATH}")
        return manifest

    campaign.RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    configs = campaign.discover_runnable_configs()
    entries = []
    for spec_path in configs:
        entry = campaign.manifest_entry(spec_path)
        import_existing_result(spec_path, entry)
        entries.append(entry)

    ivy_path = (
        campaign.REPO_ROOT
        / "assets"
        / "scenes"
        / "unpacked"
        / "main_sponza_ivy"
        / "NewSponza_Main_Ivy_glTF.gltf"
    )
    manifest = {
        "schema_version": 1,
        "campaign_status": "running",
        "started_at": campaign.now_iso(),
        "finished_at": None,
        "last_updated_at": campaign.now_iso(),
        "repository_root": str(campaign.REPO_ROOT),
        "results_dir": campaign.relative_to_repo(campaign.RESULTS_DIR),
        "results_backup": None,
        "hardware": "NVIDIA GeForce RTX 5060 Ti 16GB",
        "archive_hardware_note": (
            "Earlier archived results under datas/ use NVIDIA GeForce RTX 5070"
        ),
        "locked_runnable_configs": [path.name for path in configs],
        "config_count": len(configs),
        "expected_runs": sum(entry["expected_runs"] for entry in entries),
        "successful_runs": 0,
        "salvaged_runs": 0,
        "failed_runs": 0,
        "skipped_runs": 0,
        "ivy": {
            "exists": ivy_path.exists() and ivy_path.is_file(),
            "path": campaign.relative_to_repo(ivy_path),
        },
        "configs": entries,
    }
    campaign.update_campaign_totals(manifest)
    campaign.write_json_atomic(campaign.MANIFEST_PATH, manifest)
    print(
        f"Initialized {len(configs)} configs, "
        f"{manifest['expected_runs']} expected runs; "
        f"imported {manifest['successful_runs']} successful smoke runs."
    )
    return manifest


campaign.initialize_campaign = initialize_campaign


if __name__ == "__main__":
    raise SystemExit(campaign.main())
