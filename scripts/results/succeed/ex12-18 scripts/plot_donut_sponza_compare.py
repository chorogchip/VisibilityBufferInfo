#!/usr/bin/env python3
"""Plot the ex12 Donut renderer comparison for Sponza."""

from __future__ import annotations

import argparse
import csv
import math
import zipfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np


GPU_LABEL = "NVIDIA GeForce RTX 5060 Ti 16GB"
SCENE_LABEL = "Sponza"
ARTIFACT_STEM = "donut_sponza"
ARCHIVE_SCRIPT_PATH: Path | None = None

RENDERERS = {
    "DonutDeferred": {
        "variant": 7,
        "label": "Deferred",
        "color": "#4C78A8",
        "passes": ("geometry", "lighting", "tonemap"),
    },
    "DonutDeferredPrepass": {
        "variant": 8,
        "label": "Deferred + Prepass",
        "color": "#F58518",
        "passes": ("depth_prepass", "geometry", "lighting", "tonemap"),
    },
    "DonutVisGBuffer": {
        "variant": 9,
        "label": "VisBuf + G-buffer",
        "color": "#54A24B",
        "passes": ("visibility", "gbuffer", "lighting", "tonemap"),
    },
}

PASS_ORDER = (
    "depth_prepass",
    "geometry",
    "visibility",
    "gbuffer",
    "lighting",
    "tonemap",
    "unprofiled",
)

PASS_LABELS = {
    "depth_prepass": "Depth pre-pass",
    "geometry": "Geometry / G-buffer fill",
    "visibility": "Visibility",
    "gbuffer": "VisBuf resolve to G-buffer",
    "lighting": "Lighting",
    "tonemap": "Tonemap",
    "unprofiled": "Unprofiled / timestamp gap",
}

PASS_COLORS = {
    "depth_prepass": "#E45756",
    "geometry": "#4C78A8",
    "visibility": "#72B7B2",
    "gbuffer": "#54A24B",
    "lighting": "#F2CF5B",
    "tonemap": "#B279A2",
    "unprofiled": "#BAB0AC",
}


def parse_args() -> argparse.Namespace:
    script_dir = Path(__file__).resolve().parent
    live_result_dir = script_dir / "results" / "ex12_donut_sponza_compare"
    archived_result_dir = (
        script_dir
        / "results"
        / "succeed"
        / "ex12_donut_sponza_compare"
    )
    default_result_dir = (
        live_result_dir if live_result_dir.exists() else archived_result_dir
    )

    parser = argparse.ArgumentParser(
        description="Plot Donut Deferred, Prepass, and VisBuf results for Sponza."
    )
    parser.add_argument(
        "--result-dir",
        type=Path,
        default=default_result_dir,
        help="Directory produced by scripts/run.py.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Plot output directory. Defaults to <result-dir>/plots.",
    )
    return parser.parse_args()


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise ValueError(f"CSV is empty: {path}")
    return rows


def require_float(row: dict[str, str], field: str) -> float:
    text = row.get(field, "").strip()
    if not text:
        raise ValueError(f"Missing value for {field}")
    value = float(text)
    if not math.isfinite(value):
        raise ValueError(f"Non-finite value for {field}: {text}")
    return value


def load_aggregate(result_dir: Path) -> dict[str, dict[str, str]]:
    path = result_dir / f"{result_dir.name}.csv"
    rows = read_rows(path)
    result: dict[str, dict[str, str]] = {}

    for row in rows:
        renderer_name = row.get("renderer_name", "")
        if renderer_name not in RENDERERS:
            continue
        if renderer_name in result:
            raise ValueError(f"Duplicate aggregate row for {renderer_name}")

        expected_variant = RENDERERS[renderer_name]["variant"]
        actual_variant = int(row["param_renderer_variant"])
        if actual_variant != expected_variant:
            raise ValueError(
                f"{renderer_name} has variant {actual_variant}, "
                f"expected {expected_variant}"
            )

        status = row.get("runner_status", "")
        return_code = row.get("runner_return_code", "")
        error = row.get("runner_error", "").strip()
        if return_code != "0":
            raise ValueError(f"{renderer_name} return code is {return_code}")
        if status == "salvaged" and not error.startswith("Log saved to:"):
            raise ValueError(f"{renderer_name} has a real runner error: {error}")
        if status not in {"success", "salvaged"}:
            raise ValueError(f"{renderer_name} has invalid status: {status}")

        result[renderer_name] = row

    missing = set(RENDERERS) - set(result)
    if missing:
        raise ValueError(f"Missing aggregate renderer rows: {sorted(missing)}")
    return result


def pass_averages(row: dict[str, str]) -> dict[str, float]:
    values: dict[str, float] = {}
    for index in range(32):
        name = row.get(f"pass_name_{index}", "").strip()
        if not name or name == "total":
            continue
        values[name] = require_float(row, f"pass_{index}_time_avg_ms")
    return values


def load_profile(
    runs_dir: Path,
    aggregate_row: dict[str, str],
    renderer_name: str,
) -> list[dict[str, str]]:
    run_index = int(aggregate_row["runner_run_index"])
    matches = sorted(runs_dir.glob(f"run_{run_index:05d}.csv_*_result.csv"))
    if len(matches) != 1:
        raise ValueError(
            f"Expected one profile CSV for {renderer_name}, found {matches}"
        )

    rows = read_rows(matches[0])
    required_fields = {
        "frame",
        "total",
        "index_count",
        *RENDERERS[renderer_name]["passes"],
    }
    missing_fields = required_fields - set(rows[0])
    if missing_fields:
        raise ValueError(
            f"{matches[0].name} is missing fields: {sorted(missing_fields)}"
        )

    measure_frames = int(aggregate_row["param_measure_frames"])
    window_frames = int(aggregate_row["param_profile_window_frames"])
    if measure_frames % window_frames:
        raise ValueError("measure_frames is not divisible by profile_window_frames")
    expected_rows = measure_frames // window_frames
    if len(rows) != expected_rows:
        raise ValueError(
            f"{renderer_name} profile has {len(rows)} rows, expected {expected_rows}"
        )

    for row in rows:
        for field in required_fields - {"frame"}:
            value = require_float(row, field)
            if value < 0:
                raise ValueError(
                    f"{renderer_name} has a negative {field} value: {value}"
                )
    return rows


def validate_common_workload(
    profiles: dict[str, list[dict[str, str]]],
) -> tuple[list[int], list[float]]:
    reference_name = next(iter(RENDERERS))
    reference_frames = [
        int(row["frame"]) for row in profiles[reference_name]
    ]
    reference_index_counts = [
        require_float(row, "index_count") for row in profiles[reference_name]
    ]

    for renderer_name in list(RENDERERS)[1:]:
        frames = [int(row["frame"]) for row in profiles[renderer_name]]
        index_counts = [
            require_float(row, "index_count")
            for row in profiles[renderer_name]
        ]
        if frames != reference_frames:
            raise ValueError(
                f"Profile frame sequence differs for {renderer_name}"
            )
        if index_counts != reference_index_counts:
            raise ValueError(
                f"VFC index_count profile differs for {renderer_name}"
            )

    return reference_frames, reference_index_counts


def experiment_subtitle(aggregate: dict[str, dict[str, str]]) -> str:
    row = aggregate["DonutDeferred"]
    width = row["param_window_width"]
    height = row["param_window_height"]
    measure_frames = row["param_measure_frames"]
    window_frames = row["param_profile_window_frames"]
    parts = []
    if GPU_LABEL:
        parts.append(GPU_LABEL)
    parts.extend(
        (
            f"{width}x{height}",
            f"{measure_frames} measured frames",
            f"{window_frames}-frame profile windows",
            "VFC on",
            "textures on",
        )
    )
    return " | ".join(parts)


def style_figure(figure: plt.Figure, title: str, subtitle: str) -> None:
    figure.suptitle(title, fontsize=17, fontweight="bold", y=0.985)
    figure.text(
        0.5,
        0.947,
        subtitle,
        ha="center",
        va="top",
        fontsize=10.5,
        color="#444444",
    )


def draw_profile(
    axis: plt.Axes,
    frames: list[int],
    profiles: dict[str, list[dict[str, str]]],
) -> None:
    for renderer_name, config in RENDERERS.items():
        totals = [
            require_float(row, "total") for row in profiles[renderer_name]
        ]
        axis.plot(
            frames,
            totals,
            label=config["label"],
            color=config["color"],
            linewidth=1.8,
        )

    axis.set_title("Total GPU time along the camera path")
    axis.set_xlabel("Measured frame")
    axis.set_ylabel("GPU time (ms)")
    axis.grid(True, alpha=0.28)
    axis.legend(ncol=3, frameon=True)


def total_metrics(
    aggregate: dict[str, dict[str, str]],
) -> dict[str, list[float]]:
    fields = {
        "Average": "total_time_avg_ms",
        "Median": "total_time_median_ms",
        "P90": "total_time_p90_ms",
        "P99": "total_time_p99_ms",
    }
    return {
        metric: [
            require_float(aggregate[renderer_name], field)
            for renderer_name in RENDERERS
        ]
        for metric, field in fields.items()
    }


def draw_total_statistics(
    axis: plt.Axes,
    aggregate: dict[str, dict[str, str]],
) -> None:
    metrics = total_metrics(aggregate)
    x = np.arange(len(RENDERERS))
    width = 0.19

    for offset, (metric, values) in enumerate(metrics.items()):
        positions = x + (offset - 1.5) * width
        bars = axis.bar(positions, values, width, label=metric)
        axis.bar_label(bars, fmt="%.2f", padding=2, fontsize=8)

    axis.set_title("Total GPU time statistics")
    axis.set_xticks(
        x,
        [config["label"] for config in RENDERERS.values()],
    )
    axis.set_ylabel("GPU time (ms)")
    axis.grid(True, axis="y", alpha=0.28)
    axis.legend(ncol=4, frameon=True)


def pass_breakdown(
    aggregate: dict[str, dict[str, str]],
) -> dict[str, dict[str, float]]:
    result: dict[str, dict[str, float]] = {}
    for renderer_name, config in RENDERERS.items():
        row = aggregate[renderer_name]
        renderer_passes = pass_averages(row)
        missing = set(config["passes"]) - set(renderer_passes)
        if missing:
            raise ValueError(
                f"{renderer_name} is missing pass averages: {sorted(missing)}"
            )

        total = require_float(row, "total_time_avg_ms")
        profiled = sum(renderer_passes[name] for name in config["passes"])
        unprofiled = total - profiled
        if unprofiled < -0.001:
            raise ValueError(
                f"{renderer_name} pass sum exceeds total by {-unprofiled:.6f} ms"
            )

        result[renderer_name] = {
            **renderer_passes,
            "unprofiled": max(0.0, unprofiled),
        }
    return result


def draw_pass_breakdown(
    axis: plt.Axes,
    aggregate: dict[str, dict[str, str]],
) -> None:
    breakdown = pass_breakdown(aggregate)
    x = np.arange(len(RENDERERS))
    bottom = np.zeros(len(RENDERERS))

    for pass_name in PASS_ORDER:
        values = np.array(
            [
                breakdown[renderer_name].get(pass_name, 0.0)
                for renderer_name in RENDERERS
            ]
        )
        if not np.any(values):
            continue
        axis.bar(
            x,
            values,
            bottom=bottom,
            label=PASS_LABELS[pass_name],
            color=PASS_COLORS[pass_name],
            width=0.66,
        )
        bottom += values

    for index, renderer_name in enumerate(RENDERERS):
        total = require_float(
            aggregate[renderer_name], "total_time_avg_ms"
        )
        axis.text(
            index,
            total + 0.04,
            f"{total:.2f} ms",
            ha="center",
            va="bottom",
            fontsize=9,
            fontweight="bold",
        )

    axis.set_title("Average pass composition")
    axis.set_xticks(
        x,
        [config["label"] for config in RENDERERS.values()],
    )
    axis.set_ylabel("Average GPU time (ms)")
    axis.grid(True, axis="y", alpha=0.28)
    axis.set_ylim(0, float(max(bottom)) * 1.32)
    axis.legend(ncol=2, frameon=True, fontsize=8.5)


def save_component_plots(
    output_dir: Path,
    subtitle: str,
    frames: list[int],
    aggregate: dict[str, dict[str, str]],
    profiles: dict[str, list[dict[str, str]]],
) -> list[Path]:
    created: list[Path] = []

    figure, axis = plt.subplots(figsize=(14, 7.2))
    draw_profile(axis, frames, profiles)
    style_figure(
        figure,
        f"{SCENE_LABEL}: Donut renderer comparison",
        subtitle,
    )
    figure.tight_layout(rect=(0, 0, 1, 0.91))
    path = output_dir / "total_gpu_time_by_camera_frame.png"
    figure.savefig(path, dpi=180, bbox_inches="tight")
    plt.close(figure)
    created.append(path)

    figure, axis = plt.subplots(figsize=(11.5, 7.2))
    draw_total_statistics(axis, aggregate)
    style_figure(
        figure,
        f"{SCENE_LABEL}: total GPU time statistics",
        subtitle,
    )
    figure.tight_layout(rect=(0, 0, 1, 0.91))
    path = output_dir / "total_gpu_time_statistics.png"
    figure.savefig(path, dpi=180, bbox_inches="tight")
    plt.close(figure)
    created.append(path)

    figure, axis = plt.subplots(figsize=(11.5, 7.2))
    draw_pass_breakdown(axis, aggregate)
    style_figure(
        figure,
        f"{SCENE_LABEL}: Donut pass breakdown",
        subtitle,
    )
    figure.tight_layout(rect=(0, 0, 1, 0.91))
    path = output_dir / "average_pass_breakdown.png"
    figure.savefig(path, dpi=180, bbox_inches="tight")
    plt.close(figure)
    created.append(path)

    return created


def save_overview(
    output_dir: Path,
    subtitle: str,
    frames: list[int],
    aggregate: dict[str, dict[str, str]],
    profiles: dict[str, list[dict[str, str]]],
) -> Path:
    figure = plt.figure(figsize=(16, 11))
    grid = figure.add_gridspec(2, 2, height_ratios=(1.1, 1.0))
    profile_axis = figure.add_subplot(grid[0, :])
    statistics_axis = figure.add_subplot(grid[1, 0])
    breakdown_axis = figure.add_subplot(grid[1, 1])

    draw_profile(profile_axis, frames, profiles)
    draw_total_statistics(statistics_axis, aggregate)
    draw_pass_breakdown(breakdown_axis, aggregate)
    style_figure(
        figure,
        f"{SCENE_LABEL}: Donut Deferred vs Visibility Buffer",
        subtitle,
    )
    figure.tight_layout(rect=(0, 0, 1, 0.925), h_pad=3.0, w_pad=2.5)

    path = output_dir / f"{ARTIFACT_STEM}_comparison_overview.png"
    figure.savefig(path, dpi=180, bbox_inches="tight")
    plt.close(figure)
    return path


def write_summary_csv(
    path: Path,
    aggregate: dict[str, dict[str, str]],
    index_counts: list[float],
) -> None:
    fields = [
        "gpu",
        "scene",
        "resolution",
        "warmup_frames",
        "measure_frames",
        "profile_window_frames",
        "vfc",
        "textures",
        "renderer_variant",
        "renderer_name",
        "plot_label",
        "total_avg_ms",
        "total_median_ms",
        "total_p90_ms",
        "total_p99_ms",
        "total_reduction_vs_deferred_pct",
        "total_reduction_vs_prepass_pct",
        "index_count_profile_identical",
        "index_count_min",
        "index_count_max",
        *[f"{name}_avg_ms" for name in PASS_ORDER],
    ]

    deferred_avg = require_float(
        aggregate["DonutDeferred"], "total_time_avg_ms"
    )
    prepass_avg = require_float(
        aggregate["DonutDeferredPrepass"], "total_time_avg_ms"
    )

    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()

        for renderer_name, config in RENDERERS.items():
            row = aggregate[renderer_name]
            average = require_float(row, "total_time_avg_ms")
            passes = pass_breakdown(aggregate)[renderer_name]
            writer.writerow(
                {
                    "gpu": GPU_LABEL,
                    "scene": SCENE_LABEL,
                    "resolution": (
                        f"{row['param_window_width']}x"
                        f"{row['param_window_height']}"
                    ),
                    "warmup_frames": row["param_warmup_frames"],
                    "measure_frames": row["param_measure_frames"],
                    "profile_window_frames": (
                        row["param_profile_window_frames"]
                    ),
                    "vfc": row["param_use_vfc"],
                    "textures": row["param_to_load_texture"],
                    "renderer_variant": config["variant"],
                    "renderer_name": renderer_name,
                    "plot_label": config["label"],
                    "total_avg_ms": f"{average:.6f}",
                    "total_median_ms": (
                        f"{require_float(row, 'total_time_median_ms'):.6f}"
                    ),
                    "total_p90_ms": (
                        f"{require_float(row, 'total_time_p90_ms'):.6f}"
                    ),
                    "total_p99_ms": (
                        f"{require_float(row, 'total_time_p99_ms'):.6f}"
                    ),
                    "total_reduction_vs_deferred_pct": (
                        f"{(deferred_avg - average) / deferred_avg * 100:.6f}"
                    ),
                    "total_reduction_vs_prepass_pct": (
                        f"{(prepass_avg - average) / prepass_avg * 100:.6f}"
                    ),
                    "index_count_profile_identical": "true",
                    "index_count_min": f"{min(index_counts):.0f}",
                    "index_count_max": f"{max(index_counts):.0f}",
                    **{
                        f"{name}_avg_ms": (
                            f"{passes.get(name, 0.0):.6f}"
                        )
                        for name in PASS_ORDER
                    },
                }
            )


def create_zip(
    zip_path: Path,
    created: list[Path],
    script_path: Path,
) -> None:
    with zipfile.ZipFile(
        zip_path,
        mode="w",
        compression=zipfile.ZIP_DEFLATED,
    ) as archive:
        for path in created:
            archive.write(path, arcname=f"plots/{path.name}")
        archive.write(script_path, arcname=script_path.name)
        helper_path = Path(__file__).resolve()
        if helper_path != script_path:
            archive.write(helper_path, arcname=helper_path.name)


def main() -> int:
    args = parse_args()
    result_dir = args.result_dir.resolve()
    output_dir = (
        args.output_dir.resolve()
        if args.output_dir
        else result_dir / "plots"
    )
    output_dir.mkdir(parents=True, exist_ok=True)

    aggregate = load_aggregate(result_dir)
    runs_dir = result_dir / f"{result_dir.name}_runs"
    profiles = {
        renderer_name: load_profile(
            runs_dir,
            aggregate[renderer_name],
            renderer_name,
        )
        for renderer_name in RENDERERS
    }
    frames, index_counts = validate_common_workload(profiles)
    subtitle = experiment_subtitle(aggregate)

    plt.style.use("seaborn-v0_8-whitegrid")
    created = save_component_plots(
        output_dir,
        subtitle,
        frames,
        aggregate,
        profiles,
    )
    created.append(
        save_overview(
            output_dir,
            subtitle,
            frames,
            aggregate,
            profiles,
        )
    )

    summary_path = output_dir / f"{ARTIFACT_STEM}_summary.csv"
    write_summary_csv(summary_path, aggregate, index_counts)
    created.append(summary_path)

    zip_path = result_dir / f"{ARTIFACT_STEM}_comparison_plots.zip"
    create_zip(
        zip_path,
        created,
        ARCHIVE_SCRIPT_PATH or Path(__file__).resolve(),
    )

    if GPU_LABEL:
        print(f"GPU: {GPU_LABEL}")
    print(
        f"Validated common workload: {len(frames)} profile windows, "
        f"index count {min(index_counts):.0f}..{max(index_counts):.0f}"
    )
    for renderer_name, config in RENDERERS.items():
        row = aggregate[renderer_name]
        print(
            f"{config['label']}: "
            f"avg={require_float(row, 'total_time_avg_ms'):.5f} ms, "
            f"median={require_float(row, 'total_time_median_ms'):.5f} ms, "
            f"p90={require_float(row, 'total_time_p90_ms'):.5f} ms, "
            f"p99={require_float(row, 'total_time_p99_ms'):.5f} ms"
        )
    print(f"Created {len(created)} files in {output_dir}")
    print(f"ZIP: {zip_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
