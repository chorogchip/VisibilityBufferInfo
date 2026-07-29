#!/usr/bin/env python3
"""Audit material experiment results and generate reproducible plots."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import shutil
import statistics
from collections import defaultdict
from pathlib import Path
from typing import Any, Callable, Iterable, Sequence

import matplotlib

matplotlib.use("Agg")
matplotlib.rcParams["svg.hashsalt"] = "tvb-material-experiments-20260729"
import matplotlib.pyplot as plt


SCRIPT_DIR = Path(__file__).resolve().parent
RESULTS_DIR = SCRIPT_DIR / "results"
PLOTS_DIR = SCRIPT_DIR / "plots"
DATA_DIR = PLOTS_DIR / "data"
PNG_DIR = PLOTS_DIR / "png"
SVG_DIR = PLOTS_DIR / "svg"
MANIFEST_PATH = RESULTS_DIR / "_campaign_manifest.json"

SUCCESS_STATUS = "success"
REQUIRED_PROGRAM_FIELDS = (
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
TOTAL_METRICS = (
    "total_time_avg_ms",
    "total_time_median_ms",
    "total_time_p90_ms",
    "total_time_p99_ms",
)
PASS_COUNT = 32
COLORS = (
    "#0072B2",
    "#D55E00",
    "#009E73",
    "#CC79A7",
    "#E69F00",
    "#56B4E9",
    "#F0E442",
    "#000000",
)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig") as file:
        value = json.load(file)
    if not isinstance(value, dict):
        raise ValueError(f"Expected a JSON object: {path}")
    return value


def read_csv(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open("r", encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        return list(reader.fieldnames or []), list(reader)


def write_csv(
    path: Path,
    rows: Sequence[dict[str, Any]],
    fieldnames: Sequence[str],
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8-sig", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as file:
        json.dump(value, file, ensure_ascii=False, indent=2)
        file.write("\n")


def as_float(value: Any) -> float:
    result = float(str(value))
    if not math.isfinite(result):
        raise ValueError(f"Non-finite numeric value: {value!r}")
    return result


def maybe_float(value: Any) -> float | None:
    try:
        return as_float(value)
    except (TypeError, ValueError):
        return None


def scene_name(row: dict[str, str]) -> str:
    if row.get("param_to_use_scene", "0") in {"0", "false", "False"}:
        return "Synthetic"
    path = row.get("param_scene_path", "").replace("\\", "/").lower()
    if "main_sponza_ivy" in path or "ivy" in Path(path).name:
        return "SponzaIvy"
    if "bistro" in path:
        return "Bistro"
    if "sponza" in path:
        return "Sponza"
    return Path(path).stem or "Unknown"


def short_config(config: str) -> str:
    return config.removesuffix(".json")


def renderer_name(row: dict[str, str]) -> str:
    return row.get("renderer_name") or (
        "Variant " + row.get("param_renderer_variant", "?")
    )


def mean_std(values: Iterable[float]) -> tuple[float, float]:
    items = list(values)
    if not items:
        raise ValueError("Cannot aggregate an empty value list.")
    return statistics.fmean(items), (
        statistics.pstdev(items) if len(items) > 1 else 0.0
    )


def reset_generated_outputs() -> None:
    plot_root = PLOTS_DIR.resolve()
    for path in (DATA_DIR, PNG_DIR, SVG_DIR):
        resolved = path.resolve()
        if resolved.parent != plot_root:
            raise RuntimeError(f"Refusing to reset unexpected plot path: {resolved}")
        if resolved.exists():
            shutil.rmtree(resolved)
        resolved.mkdir(parents=True)


def load_results() -> tuple[
    dict[str, Any],
    dict[str, list[dict[str, str]]],
    dict[str, dict[str, Any]],
    dict[str, list[str]],
]:
    manifest = read_json(MANIFEST_PATH)
    rows_by_config: dict[str, list[dict[str, str]]] = {}
    specs: dict[str, dict[str, Any]] = {}
    headers: dict[str, list[str]] = {}
    for entry in manifest["configs"]:
        config = entry["config"]
        stem = short_config(config)
        csv_path = RESULTS_DIR / stem / f"{stem}.csv"
        spec_path = SCRIPT_DIR / config
        header, rows = read_csv(csv_path)
        for row in rows:
            row["_config"] = config
            row["_description"] = str(
                read_json(spec_path).get("_description", "")
            )
            row["_scene"] = scene_name(row)
            row["_renderer"] = renderer_name(row)
        rows_by_config[config] = rows
        specs[config] = read_json(spec_path)
        headers[config] = header
    return manifest, rows_by_config, specs, headers


def audit_results(
    manifest: dict[str, Any],
    rows_by_config: dict[str, list[dict[str, str]]],
    headers: dict[str, list[str]],
) -> dict[str, Any]:
    errors: list[str] = []
    warnings: list[str] = []
    config_summaries: list[dict[str, Any]] = []
    failed_skipped: list[dict[str, Any]] = []
    manifest_entries = {
        entry["config"]: entry for entry in manifest.get("configs", [])
    }

    for config, rows in rows_by_config.items():
        entry = manifest_entries[config]
        expected = int(entry["expected_runs"])
        statuses: dict[str, int] = defaultdict(int)
        indexes: list[int] = []
        signatures: list[tuple[tuple[str, str], ...]] = []
        config_errors: list[str] = []
        header = headers[config]

        missing_header = [
            field for field in REQUIRED_PROGRAM_FIELDS if field not in header
        ]
        if missing_header:
            config_errors.append(
                "missing ProgramResult headers: " + ", ".join(missing_header)
            )

        parameter_fields = sorted(
            field for field in header if field.startswith("param_")
        )
        for row_number, row in enumerate(rows, start=2):
            status = row.get("runner_status", "")
            statuses[status] += 1
            if status != SUCCESS_STATUS:
                failed_skipped.append(
                    {
                        "config": config,
                        "run_index": row.get("runner_run_index", ""),
                        "runner_status": status,
                        "runner_error": row.get("runner_error", ""),
                        "runner_skip_reason": row.get(
                            "runner_skip_reason", ""
                        ),
                        "runner_missing_assets": row.get(
                            "runner_missing_assets", ""
                        ),
                    }
                )
                continue

            try:
                indexes.append(int(row["runner_run_index"]))
            except (KeyError, ValueError):
                config_errors.append(f"row {row_number}: invalid run index")

            if row.get("runner_error", "").strip():
                config_errors.append(
                    f"row {row_number}: success has runner_error"
                )

            missing_values = [
                field
                for field in REQUIRED_PROGRAM_FIELDS
                if not row.get(field, "").strip()
            ]
            if missing_values:
                config_errors.append(
                    f"row {row_number}: empty required values "
                    + ", ".join(missing_values)
                )

            for metric in TOTAL_METRICS:
                value = maybe_float(row.get(metric))
                if value is None or value <= 0:
                    config_errors.append(
                        f"row {row_number}: invalid {metric}="
                        f"{row.get(metric)!r}"
                    )

            for pass_index in range(PASS_COUNT):
                pass_name = row.get(f"pass_name_{pass_index}", "").strip()
                pass_time = row.get(
                    f"pass_{pass_index}_time_avg_ms", ""
                ).strip()
                if pass_name:
                    value = maybe_float(pass_time)
                    if value is None or value < 0:
                        config_errors.append(
                            f"row {row_number}: invalid pass timing "
                            f"{pass_name}={pass_time!r}"
                        )

            signatures.append(
                tuple(
                    [(field, row.get(field, "")) for field in parameter_fields]
                    + [("runner_repeat", row.get("runner_repeat", ""))]
                )
            )

        unique_indexes = set(indexes)
        duplicate_indexes = len(indexes) - len(unique_indexes)
        duplicate_conditions = len(signatures) - len(set(signatures))
        expected_indexes = set(range(expected))
        missing_indexes = sorted(expected_indexes - unique_indexes)

        if len(rows) != expected:
            config_errors.append(f"CSV rows {len(rows)} != expected {expected}")
        if duplicate_indexes:
            config_errors.append(f"duplicate run indexes: {duplicate_indexes}")
        if missing_indexes:
            config_errors.append(
                "missing run indexes: " + ", ".join(map(str, missing_indexes))
            )
        if duplicate_conditions:
            config_errors.append(
                f"duplicate full parameter conditions: {duplicate_conditions}"
            )
        if statuses.get(SUCCESS_STATUS, 0) != int(
            entry["successful_runs"]
        ):
            config_errors.append("CSV success count disagrees with manifest")
        if sum(statuses.values()) != expected:
            config_errors.append("CSV status total disagrees with expected")

        report_path = (
            RESULTS_DIR
            / short_config(config)
            / f"{short_config(config)}_run_report.json"
        )
        report = read_json(report_path)
        if int(report.get("completed_runs", -1)) != expected:
            config_errors.append("run report completed count is inconsistent")
        report_runs = {
            int(run["run_index"]): run for run in report.get("runs", [])
        }
        for row in rows:
            if row.get("runner_status") != SUCCESS_STATUS:
                continue
            run_index = int(row["runner_run_index"])
            parameters = report_runs.get(run_index, {}).get("parameters", {})
            for name, value in parameters.items():
                field = f"param_{name}"
                if field not in row:
                    config_errors.append(
                        f"run {run_index}: missing parameter column {field}"
                    )
                    continue
                rendered = (
                    "1"
                    if value is True
                    else "0"
                    if value is False
                    else str(value)
                )
                if row[field] != rendered:
                    config_errors.append(
                        f"run {run_index}: {field}={row[field]!r}, "
                        f"expected {rendered!r}"
                    )

        primary_headers: set[tuple[str, ...]] = set()
        runs_dir = (
            RESULTS_DIR / short_config(config) / f"{short_config(config)}_runs"
        )
        for individual in runs_dir.glob("run_[0-9][0-9][0-9][0-9][0-9].csv"):
            individual_header, _ = read_csv(individual)
            primary_headers.add(tuple(individual_header))
        if len(primary_headers) > 1:
            config_errors.append("individual ProgramResult schemas differ")

        errors.extend(f"{config}: {error}" for error in sorted(set(config_errors)))
        config_summaries.append(
            {
                "config": config,
                "expected_runs": expected,
                "csv_rows": len(rows),
                "successful_runs": statuses.get(SUCCESS_STATUS, 0),
                "salvaged_runs": statuses.get("salvaged", 0),
                "failed_runs": statuses.get("failed", 0),
                "skipped_runs": sum(
                    count
                    for status, count in statuses.items()
                    if status.startswith("skipped")
                ),
                "duplicate_run_indexes": duplicate_indexes,
                "missing_run_indexes": len(missing_indexes),
                "duplicate_conditions": duplicate_conditions,
                "schema_columns": len(header),
                "validation_errors": len(set(config_errors)),
                "status": "passed" if not config_errors else "failed",
            }
        )

    expected_total = int(manifest["expected_runs"])
    actual_total = sum(len(rows) for rows in rows_by_config.values())
    if actual_total != expected_total:
        errors.append(
            f"campaign CSV row total {actual_total} != expected {expected_total}"
        )
    if int(manifest["successful_runs"]) != expected_total:
        errors.append("campaign manifest is not entirely successful")
    if manifest.get("final_errors"):
        errors.extend(
            f"manifest final error: {value}"
            for value in manifest["final_errors"]
        )

    backup_dirs = sorted(
        path.name
        for path in SCRIPT_DIR.glob("results_backup_*")
        if path.is_dir()
    )
    if backup_dirs:
        warnings.append(
            "Backup directories were preserved outside results: "
            + ", ".join(backup_dirs)
        )

    capture_root = (
        RESULTS_DIR
        / "31_capture_representative_frames"
        / "31_capture_representative_frames_runs"
    )
    sponza_frames = sorted(
        (capture_root / "run_00002_capture" / "frames").glob("*.png")
    )
    ivy_frames = sorted(
        (capture_root / "run_00006_capture" / "frames").glob("*.png")
    )
    if sponza_frames and len(sponza_frames) == len(ivy_frames):
        identical = sum(
            hashlib.sha256(a.read_bytes()).digest()
            == hashlib.sha256(b.read_bytes()).digest()
            for a, b in zip(sponza_frames, ivy_frames)
        )
        if identical >= len(sponza_frames) - 1:
            warnings.append(
                f"{identical}/{len(sponza_frames)} sampled Sponza and "
                "SponzaIvy capture frames are byte-identical on the current "
                "camera path; the assets and timings differ, but visibly "
                "distinct ivy was not captured."
            )

    return {
        "status": "passed" if not errors else "failed",
        "campaign_expected_runs": expected_total,
        "campaign_csv_rows": actual_total,
        "successful_rows": sum(
            summary["successful_runs"] for summary in config_summaries
        ),
        "salvaged_rows": sum(
            summary["salvaged_runs"] for summary in config_summaries
        ),
        "failed_rows": sum(
            summary["failed_runs"] for summary in config_summaries
        ),
        "skipped_rows": sum(
            summary["skipped_runs"] for summary in config_summaries
        ),
        "config_summaries": config_summaries,
        "failed_skipped_cases": failed_skipped,
        "errors": sorted(set(errors)),
        "warnings": warnings,
    }


class PlotWriter:
    def __init__(self) -> None:
        self.index: list[dict[str, str]] = []

    def save(
        self,
        fig: plt.Figure,
        plot_id: str,
        title: str,
        configs: Sequence[str],
        *,
        filters: str,
        x: str,
        series: str,
    ) -> None:
        fig.suptitle(title, fontsize=13)
        fig.tight_layout(rect=(0, 0, 1, 0.97))
        png_path = PNG_DIR / f"{plot_id}.png"
        svg_path = SVG_DIR / f"{plot_id}.svg"
        fig.savefig(
            png_path,
            dpi=160,
            bbox_inches="tight",
            metadata={"Software": "plot_results.py"},
        )
        fig.savefig(
            svg_path,
            bbox_inches="tight",
            metadata={"Date": None, "Creator": "plot_results.py"},
        )
        svg_text = svg_path.read_text(encoding="utf-8")
        svg_path.write_text(
            "\n".join(line.rstrip() for line in svg_text.splitlines()) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        plt.close(fig)
        self.index.append(
            {
                "plot_id": plot_id,
                "title": title,
                "input_configs": ";".join(configs),
                "filters": filters,
                "x_axis": x,
                "series": series,
                "metric": "GPU total time unless noted",
                "png": f"png/{plot_id}.png",
                "svg": f"svg/{plot_id}.svg",
            }
        )


def success_rows(
    rows_by_config: dict[str, list[dict[str, str]]],
    configs: Sequence[str],
) -> list[dict[str, str]]:
    return [
        row
        for config in configs
        for row in rows_by_config[config]
        if row.get("runner_status") == SUCCESS_STATUS
    ]


def aggregate_metric(
    rows: Sequence[dict[str, str]],
    key: Callable[[dict[str, str]], tuple[Any, ...]],
    metric: str = "total_time_avg_ms",
) -> dict[tuple[Any, ...], tuple[float, float, int]]:
    grouped: dict[tuple[Any, ...], list[float]] = defaultdict(list)
    for row in rows:
        grouped[key(row)].append(as_float(row[metric]))
    return {
        group: (*mean_std(values), len(values))
        for group, values in grouped.items()
    }


def line_plot(
    writer: PlotWriter,
    rows: Sequence[dict[str, str]],
    *,
    plot_id: str,
    title: str,
    configs: Sequence[str],
    x_key: str,
    group_label: Callable[[dict[str, str]], str],
    x_label: str,
    filters: str,
    log_x: bool = False,
) -> None:
    grouped = aggregate_metric(
        rows,
        lambda row: (group_label(row), as_float(row[x_key])),
    )
    fig, ax = plt.subplots(figsize=(8.4, 5.0))
    labels = sorted({key[0] for key in grouped})
    for color, label in zip(COLORS, labels):
        values = sorted(
            (x, mean, std)
            for (series, x), (mean, std, _) in grouped.items()
            if series == label
        )
        ax.errorbar(
            [value[0] for value in values],
            [value[1] for value in values],
            yerr=[value[2] for value in values],
            marker="o",
            linewidth=1.8,
            capsize=3,
            label=label,
            color=color,
        )
    if log_x:
        ax.set_xscale("log", base=2)
    ax.set_xlabel(x_label)
    ax.set_ylabel("GPU total time avg (ms)")
    ax.grid(True, alpha=0.28)
    if len(labels) > 1:
        ax.legend(fontsize=8)
    writer.save(
        fig,
        plot_id,
        title,
        configs,
        filters=filters,
        x=x_key,
        series="; ".join(labels),
    )


def bar_means(
    writer: PlotWriter,
    rows: Sequence[dict[str, str]],
    *,
    plot_id: str,
    title: str,
    configs: Sequence[str],
    category: Callable[[dict[str, str]], str],
    filters: str,
    x_description: str,
) -> None:
    grouped = aggregate_metric(rows, lambda row: (category(row),))
    labels = sorted(key[0] for key in grouped)
    means = [grouped[(label,)][0] for label in labels]
    stds = [grouped[(label,)][1] for label in labels]
    fig, ax = plt.subplots(figsize=(max(8, len(labels) * 0.58), 5.2))
    ax.bar(
        range(len(labels)),
        means,
        yerr=stds,
        capsize=3,
        color=[COLORS[i % len(COLORS)] for i in range(len(labels))],
    )
    ax.set_xticks(range(len(labels)), labels, rotation=35, ha="right")
    ax.set_ylabel("GPU total time avg (ms)")
    ax.grid(axis="y", alpha=0.28)
    writer.save(
        fig,
        plot_id,
        title,
        configs,
        filters=filters,
        x=x_description,
        series="mean ± population std",
    )


def generic_experiment_plots(
    writer: PlotWriter,
    rows_by_config: dict[str, list[dict[str, str]]],
    manifest: dict[str, Any],
) -> None:
    priorities = (
        "param_material_assign_locality",
        "param_material_assign_diversity",
        "param_material_assign_max_open",
        "param_geometry_div",
        "param_window_width",
        "param_seed",
        "param_renderer_variant",
        "param_variable",
    )
    for entry in manifest["configs"]:
        config = entry["config"]
        rows = success_rows(rows_by_config, [config])
        varying = [
            key
            for key in priorities
            if len({row.get(key, "") for row in rows}) > 1
        ]
        x_key = varying[0] if varying else "runner_run_index"
        groups = {
            (row["_scene"], row["_renderer"])
            for row in rows
        }
        group_label = (
            (lambda row: f"{row['_scene']} · {row['_renderer']}")
            if len(groups) > 1
            else (lambda row: "all runs")
        )
        line_plot(
            writer,
            rows,
            plot_id=f"experiment_{short_config(config)}",
            title=f"{short_config(config)} — per-experiment result",
            configs=[config],
            x_key=x_key,
            group_label=group_label,
            x_label=x_key.removeprefix("param_").replace("_", " "),
            filters="runner_status=success; no failed/skipped rows plotted",
            log_x=x_key
            in {"param_material_assign_max_open", "param_geometry_div"},
        )


def heatmap_plot(
    writer: PlotWriter,
    rows_by_config: dict[str, list[dict[str, str]]],
) -> None:
    config = "12_synth_locality_diversity_map_fixed64.json"
    rows = success_rows(rows_by_config, [config])
    grouped = aggregate_metric(
        rows,
        lambda row: (
            as_float(row["param_material_assign_diversity"]),
            as_float(row["param_material_assign_locality"]),
        ),
    )
    x_values = sorted({key[1] for key in grouped})
    y_values = sorted({key[0] for key in grouped})
    matrix = [
        [grouped[(y, x)][0] for x in x_values]
        for y in y_values
    ]
    fig, ax = plt.subplots(figsize=(7.2, 5.6))
    image = ax.imshow(matrix, origin="lower", aspect="auto", cmap="viridis")
    ax.set_xticks(range(len(x_values)), [f"{x:g}" for x in x_values])
    ax.set_yticks(range(len(y_values)), [f"{y:g}" for y in y_values])
    ax.set_xlabel("Material locality")
    ax.set_ylabel("Material diversity")
    for y_index, row_values in enumerate(matrix):
        for x_index, value in enumerate(row_values):
            ax.text(
                x_index,
                y_index,
                f"{value:.3f}",
                ha="center",
                va="center",
                fontsize=7,
                color="white" if value > statistics.fmean(sum(matrix, [])) else "black",
            )
    fig.colorbar(image, ax=ax, label="GPU total time avg (ms)")
    writer.save(
        fig,
        "locality_diversity_heatmap",
        "Synthetic locality × diversity map (64 open classes)",
        [config],
        filters="three seeds averaged; runner_status=success",
        x="material_assign_locality",
        series="heatmap rows=material_assign_diversity",
    )


def seed_plot(
    writer: PlotWriter,
    rows_by_config: dict[str, list[dict[str, str]]],
) -> None:
    config = "15_synth_seed_robustness.json"
    rows = sorted(
        success_rows(rows_by_config, [config]),
        key=lambda row: as_float(row["param_seed"]),
    )
    seeds = [int(row["param_seed"]) for row in rows]
    values = [as_float(row["total_time_avg_ms"]) for row in rows]
    mean, std = mean_std(values)
    fig, ax = plt.subplots(figsize=(8.0, 4.8))
    ax.plot(seeds, values, "o-", color=COLORS[0])
    ax.axhline(mean, color=COLORS[1], label=f"mean={mean:.4f} ms")
    ax.fill_between(
        [min(seeds), max(seeds)],
        mean - std,
        mean + std,
        color=COLORS[1],
        alpha=0.16,
        label=f"±σ={std:.4f} ms",
    )
    ax.set_xlabel("Seed")
    ax.set_ylabel("GPU total time avg (ms)")
    ax.grid(True, alpha=0.28)
    ax.legend()
    writer.save(
        fig,
        "seed_robustness",
        "Synthetic seed robustness",
        [config],
        filters="fixed locality=0.5, diversity=0.5; success rows only",
        x="seed",
        series="individual seed, mean, population std",
    )


def renderer_comparison_plot(
    writer: PlotWriter,
    rows_by_config: dict[str, list[dict[str, str]]],
) -> None:
    configs = [
        "16_synth_renderer_compare_selected.json",
        "22_real_pbr_feature_renderer_compare_full.json",
    ]
    rows = success_rows(rows_by_config, configs)
    grouped = aggregate_metric(
        rows, lambda row: (row["_scene"], row["_renderer"])
    )
    scenes = ["Synthetic", "Sponza", "Bistro", "SponzaIvy"]
    scenes = [scene for scene in scenes if any(key[0] == scene for key in grouped)]
    renderers = sorted({key[1] for key in grouped})
    fig, ax = plt.subplots(figsize=(9.2, 5.2))
    width = 0.8 / max(len(renderers), 1)
    for index, renderer in enumerate(renderers):
        positions = [
            scene_index - 0.4 + width / 2 + index * width
            for scene_index in range(len(scenes))
        ]
        values = [
            grouped.get((scene, renderer), (math.nan, 0, 0))[0]
            for scene in scenes
        ]
        ax.bar(
            positions,
            values,
            width,
            label=renderer,
            color=COLORS[index % len(COLORS)],
        )
    ax.set_xticks(range(len(scenes)), scenes)
    ax.set_ylabel("GPU total time avg (ms)")
    ax.grid(axis="y", alpha=0.28)
    ax.legend(fontsize=8)
    writer.save(
        fig,
        "renderer_comparison",
        "Renderer comparison — synthetic and real scenes",
        configs,
        filters="success rows; synthetic selected conditions and real full-camera PBR",
        x="scene class",
        series="renderer_name",
    )


def workload_plot(
    writer: PlotWriter,
    rows_by_config: dict[str, list[dict[str, str]]],
) -> None:
    config = "14_synth_workload_scaling.json"
    rows = success_rows(rows_by_config, [config])
    line_plot(
        writer,
        rows,
        plot_id="workload_scaling",
        title="Synthetic workload scaling",
        configs=[config],
        x_key="param_geometry_div",
        group_label=lambda row: (
            f"{row['param_window_width']}×{row['param_window_height']} · "
            f"locality {row['param_material_assign_locality']}"
        ),
        x_label="Geometry subdivisions",
        filters="success rows; full configured resolution/locality matrix",
        log_x=True,
    )


def resolution_plot(
    writer: PlotWriter,
    rows_by_config: dict[str, list[dict[str, str]]],
) -> None:
    config = "24_real_resolution_scaling_pbr_quick.json"
    rows = success_rows(rows_by_config, [config])
    line_plot(
        writer,
        rows,
        plot_id="resolution_scaling",
        title="Real-scene resolution scaling",
        configs=[config],
        x_key="param_window_width",
        group_label=lambda row: f"{row['_scene']} · {row['_renderer']}",
        x_label="Window width (height follows configured aspect ratio)",
        filters="PBR, texture=true, VFC=true, success rows",
    )


def texture_vfc_plot(
    writer: PlotWriter,
    rows_by_config: dict[str, list[dict[str, str]]],
) -> None:
    config = "25_real_texture_vfc_ablation_quick.json"
    rows = success_rows(rows_by_config, [config])
    categories = {
        (row["param_to_load_texture"], row["param_use_vfc"])
        for row in rows
    }
    labels = sorted(categories)
    groups = sorted({(row["_scene"], row["_renderer"]) for row in rows})
    grouped = aggregate_metric(
        rows,
        lambda row: (
            row["_scene"],
            row["_renderer"],
            row["param_to_load_texture"],
            row["param_use_vfc"],
        ),
    )
    fig, ax = plt.subplots(figsize=(10.5, 5.4))
    width = 0.8 / len(groups)
    for index, (scene, renderer) in enumerate(groups):
        positions = [
            category_index - 0.4 + width / 2 + index * width
            for category_index in range(len(labels))
        ]
        values = [
            grouped[(scene, renderer, texture, vfc)][0]
            for texture, vfc in labels
        ]
        ax.bar(
            positions,
            values,
            width,
            label=f"{scene} · {renderer}",
            color=COLORS[index % len(COLORS)],
        )
    ax.set_xticks(
        range(len(labels)),
        [f"Texture {t} / VFC {v}" for t, v in labels],
    )
    ax.set_ylabel("GPU total time avg (ms)")
    ax.grid(axis="y", alpha=0.28)
    ax.legend(fontsize=7, ncol=2)
    writer.save(
        fig,
        "texture_vfc_ablation",
        "Texture loading / view-frustum culling ablation",
        [config],
        filters="success rows; all configured 2×2 ablation combinations",
        x="to_load_texture × use_vfc",
        series="scene × renderer",
    )


def pass_breakdown_plot(
    writer: PlotWriter,
    rows_by_config: dict[str, list[dict[str, str]]],
) -> None:
    config = "30_final_selected_full_camera.json"
    rows = success_rows(rows_by_config, [config])
    group_rows: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        group_rows[(row["_scene"], row["_renderer"])].append(row)
    pass_values: dict[tuple[str, str], dict[str, list[float]]] = {}
    pass_names: set[str] = set()
    for group, items in group_rows.items():
        values: dict[str, list[float]] = defaultdict(list)
        for row in items:
            for index in range(1, PASS_COUNT):
                name = row.get(f"pass_name_{index}", "").strip()
                value = row.get(f"pass_{index}_time_avg_ms", "").strip()
                if name and value:
                    values[name].append(as_float(value))
                    pass_names.add(name)
        pass_values[group] = values
    groups = sorted(group_rows)
    ordered_passes = sorted(pass_names)
    fig, ax = plt.subplots(figsize=(11.2, 5.8))
    bottoms = [0.0] * len(groups)
    for index, pass_name in enumerate(ordered_passes):
        values = [
            statistics.fmean(pass_values[group].get(pass_name, [0.0]))
            for group in groups
        ]
        ax.bar(
            range(len(groups)),
            values,
            bottom=bottoms,
            label=pass_name,
            color=COLORS[index % len(COLORS)],
        )
        bottoms = [bottom + value for bottom, value in zip(bottoms, values)]
    ax.set_xticks(
        range(len(groups)),
        [f"{scene}\n{renderer}" for scene, renderer in groups],
    )
    ax.set_ylabel("GPU pass time avg (ms)")
    ax.grid(axis="y", alpha=0.25)
    ax.legend(fontsize=7, ncol=4)
    writer.save(
        fig,
        "pass_timing_breakdown",
        "Major pass timing breakdown — final full-camera runs",
        [config],
        filters="pass slot 0 total excluded; success rows only",
        x="scene × renderer",
        series="named GPU pass",
    )


def total_statistics_plot(
    writer: PlotWriter,
    rows_by_config: dict[str, list[dict[str, str]]],
) -> None:
    config = "30_final_selected_full_camera.json"
    rows = success_rows(rows_by_config, [config])
    groups = sorted({(row["_scene"], row["_renderer"]) for row in rows})
    grouped_rows: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped_rows[(row["_scene"], row["_renderer"])].append(row)
    fig, ax = plt.subplots(figsize=(11.0, 5.6))
    width = 0.8 / len(TOTAL_METRICS)
    labels = ("avg", "median", "p90", "p99")
    for metric_index, (metric, label) in enumerate(zip(TOTAL_METRICS, labels)):
        positions = [
            index - 0.4 + width / 2 + metric_index * width
            for index in range(len(groups))
        ]
        values = [
            statistics.fmean(as_float(row[metric]) for row in grouped_rows[group])
            for group in groups
        ]
        ax.bar(
            positions,
            values,
            width,
            label=label,
            color=COLORS[metric_index],
        )
    ax.set_xticks(
        range(len(groups)),
        [f"{scene}\n{renderer}" for scene, renderer in groups],
    )
    ax.set_ylabel("GPU total time (ms)")
    ax.grid(axis="y", alpha=0.28)
    ax.legend()
    writer.save(
        fig,
        "total_time_statistics",
        "Total-time average / median / p90 / p99",
        [config],
        filters="final full-camera success rows",
        x="scene × renderer",
        series="ProgramResult total-time statistic",
    )


def status_plot(
    writer: PlotWriter,
    quality: dict[str, Any],
) -> None:
    summaries = quality["config_summaries"]
    labels = [short_config(row["config"]) for row in summaries]
    statuses = (
        ("successful_runs", "Success", COLORS[2]),
        ("salvaged_runs", "Salvaged", COLORS[4]),
        ("failed_runs", "Failed", COLORS[1]),
        ("skipped_runs", "Skipped", COLORS[3]),
    )
    fig, ax = plt.subplots(figsize=(12.0, 7.0))
    bottoms = [0] * len(labels)
    for field, label, color in statuses:
        values = [row[field] for row in summaries]
        ax.barh(range(len(labels)), values, left=bottoms, label=label, color=color)
        bottoms = [left + value for left, value in zip(bottoms, values)]
    ax.set_yticks(range(len(labels)), labels, fontsize=7)
    ax.invert_yaxis()
    ax.set_xlabel("Run count")
    ax.grid(axis="x", alpha=0.25)
    ax.legend()
    writer.save(
        fig,
        "status_summary",
        "Campaign success / salvaged / failed / skipped counts",
        [row["config"] for row in summaries],
        filters="all runner statuses, including non-measurement cases",
        x="run count",
        series="runner status",
    )


def write_data_outputs(
    manifest: dict[str, Any],
    rows_by_config: dict[str, list[dict[str, str]]],
    quality: dict[str, Any],
    writer: PlotWriter,
) -> None:
    normalized: list[dict[str, Any]] = []
    param_fields = sorted(
        {
            field
            for rows in rows_by_config.values()
            for row in rows
            for field in row
            if field.startswith("param_")
        }
    )
    pass_fields = [
        field
        for index in range(PASS_COUNT)
        for field in (
            f"pass_name_{index}",
            f"pass_{index}_time_avg_ms",
        )
    ]
    base_fields = [
        "config",
        "scene",
        "renderer_name",
        "runner_status",
        "runner_run_index",
        "runner_repeat",
        "total_time_avg_ms",
        "total_time_median_ms",
        "total_time_p90_ms",
        "total_time_p99_ms",
    ]
    normalized_fields = base_fields + param_fields + pass_fields
    for config, rows in rows_by_config.items():
        for row in rows:
            normalized.append(
                {
                    "config": config,
                    "scene": row["_scene"],
                    "renderer_name": row["_renderer"],
                    **{
                        field: row.get(field, "")
                        for field in normalized_fields
                        if field not in {"config", "scene", "renderer_name"}
                    },
                }
            )
    write_csv(
        DATA_DIR / "all_results_normalized.csv",
        normalized,
        normalized_fields,
    )
    write_csv(
        DATA_DIR / "quality_summary.csv",
        quality["config_summaries"],
        [
            "config",
            "expected_runs",
            "csv_rows",
            "successful_runs",
            "salvaged_runs",
            "failed_runs",
            "skipped_runs",
            "duplicate_run_indexes",
            "missing_run_indexes",
            "duplicate_conditions",
            "schema_columns",
            "validation_errors",
            "status",
        ],
    )
    write_csv(
        DATA_DIR / "failed_skipped_cases.csv",
        quality["failed_skipped_cases"],
        [
            "config",
            "run_index",
            "runner_status",
            "runner_error",
            "runner_skip_reason",
            "runner_missing_assets",
        ],
    )
    write_csv(
        DATA_DIR / "plot_index.csv",
        writer.index,
        [
            "plot_id",
            "title",
            "input_configs",
            "filters",
            "x_axis",
            "series",
            "metric",
            "png",
            "svg",
        ],
    )
    write_json(DATA_DIR / "quality_report.json", quality)
    write_json(DATA_DIR / "campaign_manifest_snapshot.json", manifest)


def write_readme(
    manifest: dict[str, Any],
    quality: dict[str, Any],
    writer: PlotWriter,
) -> None:
    lines = [
        "# Material experiment plots",
        "",
        "Generated reproducibly from `../results` by:",
        "",
        "```powershell",
        "python -m pip install -r scripts/material_experiments/plot_requirements.txt",
        "python scripts/material_experiments/plot_results.py",
        "```",
        "",
        "The script deletes only its generated `plots/data`, `plots/png`, and "
        "`plots/svg` directories before rebuilding them. Failed and skipped "
        "rows are listed in `data/failed_skipped_cases.csv` and are never "
        "treated as measurements.",
        "",
        "## Campaign summary",
        "",
        f"- Configs: {manifest['config_count']}",
        f"- Expected runs: {manifest['expected_runs']}",
        f"- Success: {manifest['successful_runs']}",
        f"- Salvaged: {manifest['salvaged_runs']}",
        f"- Failed: {manifest['failed_runs']}",
        f"- Skipped: {manifest['skipped_runs']}",
        f"- Quality audit: **{quality['status']}**",
        "",
        "## Quality notes",
        "",
    ]
    if quality["warnings"]:
        lines.extend(f"- {warning}" for warning in quality["warnings"])
    else:
        lines.append("- No quality warnings.")
    if quality["errors"]:
        lines.extend(f"- ERROR: {error}" for error in quality["errors"])
    lines.extend(["", "## Plot index", ""])
    for item in writer.index:
        lines.extend(
            [
                f"### {item['title']}",
                "",
                f"![{item['title']}]({item['png']})",
                "",
                f"- Inputs: `{item['input_configs']}`",
                f"- Filters: {item['filters']}",
                f"- X axis: {item['x_axis']}",
                f"- Series: {item['series']}",
                f"- SVG: [{item['plot_id']}.svg]({item['svg']})",
                "",
            ]
        )
    (PLOTS_DIR / "README.md").write_text(
        "\n".join(lines).rstrip() + "\n",
        encoding="utf-8",
        newline="\n",
    )


def generate_plots(
    manifest: dict[str, Any],
    rows_by_config: dict[str, list[dict[str, str]]],
    quality: dict[str, Any],
) -> PlotWriter:
    plt.style.use("seaborn-v0_8-whitegrid")
    writer = PlotWriter()
    generic_experiment_plots(writer, rows_by_config, manifest)

    line_plot(
        writer,
        success_rows(
            rows_by_config, ["10_synth_locality_fixed64.json"]
        ),
        plot_id="locality_change",
        title="Synthetic locality change",
        configs=["10_synth_locality_fixed64.json"],
        x_key="param_material_assign_locality",
        group_label=lambda _: "64 open classes",
        x_label="Material locality",
        filters="three seeds; diversity=1; success rows",
    )
    line_plot(
        writer,
        success_rows(
            rows_by_config, ["11_synth_diversity_fixed64.json"]
        ),
        plot_id="diversity_change",
        title="Synthetic diversity change",
        configs=["11_synth_diversity_fixed64.json"],
        x_key="param_material_assign_diversity",
        group_label=lambda row: (
            f"locality {row['param_material_assign_locality']}"
        ),
        x_label="Material diversity",
        filters="three seeds; 64 open classes; success rows",
    )
    heatmap_plot(writer, rows_by_config)
    line_plot(
        writer,
        success_rows(
            rows_by_config, ["13_synth_open_bin_count_diagnostic.json"]
        ),
        plot_id="open_bin_class_count",
        title="Synthetic material/open-bin class count",
        configs=["13_synth_open_bin_count_diagnostic.json"],
        x_key="param_material_assign_max_open",
        group_label=lambda row: (
            f"locality {row['param_material_assign_locality']}"
        ),
        x_label="Maximum open material classes",
        filters="three seeds; diversity=1; success rows",
        log_x=True,
    )
    workload_plot(writer, rows_by_config)
    seed_plot(writer, rows_by_config)
    renderer_comparison_plot(writer, rows_by_config)
    line_plot(
        writer,
        success_rows(
            rows_by_config,
            ["20_real_random_bin_count_quick_all_scenes.json"],
        ),
        plot_id="real_bin_count",
        title="Real-scene material/open-bin count",
        configs=["20_real_random_bin_count_quick_all_scenes.json"],
        x_key="param_material_assign_max_open",
        group_label=lambda row: row["_scene"],
        x_label="Maximum open material classes",
        filters="three seeds; success rows",
        log_x=True,
    )
    line_plot(
        writer,
        success_rows(
            rows_by_config,
            ["21_real_random_diversity_quick_all_scenes.json"],
        ),
        plot_id="real_diversity",
        title="Real-scene material diversity",
        configs=["21_real_random_diversity_quick_all_scenes.json"],
        x_key="param_material_assign_diversity",
        group_label=lambda row: row["_scene"],
        x_label="Material diversity",
        filters="three seeds; max open=255; success rows",
    )
    resolution_plot(writer, rows_by_config)
    texture_vfc_plot(writer, rows_by_config)
    bar_means(
        writer,
        success_rows(
            rows_by_config,
            [
                "20_real_random_bin_count_quick_all_scenes.json",
                "21_real_random_diversity_quick_all_scenes.json",
                "22_real_pbr_feature_renderer_compare_full.json",
                "23_real_random_selected_renderer_compare_quick.json",
                "24_real_resolution_scaling_pbr_quick.json",
                "25_real_texture_vfc_ablation_quick.json",
                "30_final_selected_full_camera.json",
            ],
        ),
        plot_id="scene_comparison",
        title="Scene comparison across real-scene experiments",
        configs=[
            "20_real_random_bin_count_quick_all_scenes.json",
            "21_real_random_diversity_quick_all_scenes.json",
            "22_real_pbr_feature_renderer_compare_full.json",
            "23_real_random_selected_renderer_compare_quick.json",
            "24_real_resolution_scaling_pbr_quick.json",
            "25_real_texture_vfc_ablation_quick.json",
            "30_final_selected_full_camera.json",
        ],
        category=lambda row: row["_scene"],
        filters="success rows; aggregates heterogeneous experiment families",
        x_description="scene",
    )
    bar_means(
        writer,
        success_rows(
            rows_by_config,
            [
                "10_synth_locality_fixed64.json",
                "11_synth_diversity_fixed64.json",
                "12_synth_locality_diversity_map_fixed64.json",
                "13_synth_open_bin_count_diagnostic.json",
                "14_synth_workload_scaling.json",
                "15_synth_seed_robustness.json",
                "16_synth_renderer_compare_selected.json",
            ],
        ),
        plot_id="synthetic_combined",
        title="Synthetic experiment family overview",
        configs=[
            "10_synth_locality_fixed64.json",
            "11_synth_diversity_fixed64.json",
            "12_synth_locality_diversity_map_fixed64.json",
            "13_synth_open_bin_count_diagnostic.json",
            "14_synth_workload_scaling.json",
            "15_synth_seed_robustness.json",
            "16_synth_renderer_compare_selected.json",
        ],
        category=lambda row: short_config(row["_config"]),
        filters="success rows; mean ± std per experiment",
        x_description="experiment config",
    )
    bar_means(
        writer,
        success_rows(
            rows_by_config,
            [
                "20_real_random_bin_count_quick_all_scenes.json",
                "21_real_random_diversity_quick_all_scenes.json",
                "22_real_pbr_feature_renderer_compare_full.json",
                "23_real_random_selected_renderer_compare_quick.json",
                "24_real_resolution_scaling_pbr_quick.json",
                "25_real_texture_vfc_ablation_quick.json",
                "30_final_selected_full_camera.json",
            ],
        ),
        plot_id="real_scene_combined",
        title="Real-scene experiment family overview",
        configs=[
            "20_real_random_bin_count_quick_all_scenes.json",
            "21_real_random_diversity_quick_all_scenes.json",
            "22_real_pbr_feature_renderer_compare_full.json",
            "23_real_random_selected_renderer_compare_quick.json",
            "24_real_resolution_scaling_pbr_quick.json",
            "25_real_texture_vfc_ablation_quick.json",
            "30_final_selected_full_camera.json",
        ],
        category=lambda row: short_config(row["_config"]),
        filters="success rows; mean ± std per experiment",
        x_description="experiment config",
    )
    pass_breakdown_plot(writer, rows_by_config)
    total_statistics_plot(writer, rows_by_config)
    status_plot(writer, quality)
    return writer


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--verify-only",
        action="store_true",
        help="audit results without rebuilding plots",
    )
    args = parser.parse_args()

    manifest, rows_by_config, _, headers = load_results()
    quality = audit_results(manifest, rows_by_config, headers)
    if args.verify_only:
        print(json.dumps(quality, ensure_ascii=False, indent=2))
        return 0 if quality["status"] == "passed" else 1

    reset_generated_outputs()
    writer = generate_plots(manifest, rows_by_config, quality)
    write_data_outputs(manifest, rows_by_config, quality, writer)
    write_readme(manifest, quality, writer)
    print(
        f"Generated {len(writer.index)} plots as PNG and SVG; "
        f"quality={quality['status']}."
    )
    if quality["errors"]:
        for error in quality["errors"]:
            print("ERROR:", error)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
