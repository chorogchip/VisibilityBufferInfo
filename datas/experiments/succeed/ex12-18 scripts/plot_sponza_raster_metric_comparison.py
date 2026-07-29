#!/usr/bin/env python3
"""Plot Donut renderer timings against frame-aligned raster statistics."""

from __future__ import annotations

import argparse
import csv
import math
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter, PercentFormatter


GPU_LABEL = "NVIDIA GeForce RTX 5060 Ti 16GB"
WARMUP_FRAMES = 60
WINDOW_FRAMES = 10
MEASURE_FRAMES = 2500

SCRIPT_DIR = Path(__file__).resolve().parent
RESULTS_DIR = SCRIPT_DIR / "results" / "succeed"
DEFAULT_OUTPUT_DIR = RESULTS_DIR / "ex18_sponza_raster_metric_comparison"

RENDERERS = (
    ("Deferred", 0, "#2563eb"),
    ("Prepass", 1, "#ea580c"),
    ("VisBuf", 2, "#16a34a"),
)

PASS_STYLES = {
    "geometry": "-",
    "depth_prepass": "-.",
    "visibility": "-.",
    "gbuffer": "-",
    "lighting": "--",
    "tonemap": ":",
}


@dataclass(frozen=True)
class Metric:
    key: str
    label: str
    unit: str
    scale: float = 1.0
    percent: bool = False


METRICS = (
    Metric("triangle_count", "Visible triangle count", "triangles"),
    Metric("total_fragments", "Total fragments", "fragments"),
    Metric("covered_pixels", "Covered pixels", "pixels"),
    Metric("overdraw_extra", "Extra overdraw fragments", "fragments"),
    Metric("avg_overdraw", "Average overdraw", "fragments / covered pixel"),
    Metric("max_overdraw", "Maximum overdraw", "fragments / pixel"),
    Metric("rasterized_triangles", "Rasterized triangle count", "triangles"),
    Metric("skipped_triangles", "Skipped triangle count", "triangles"),
    Metric("quad_instances", "Quad instances", "quads"),
    Metric("quad_covered_lanes", "Quad covered lanes", "lanes"),
    Metric("quad_waste_lanes", "Quad waste lanes", "lanes"),
    Metric("quad_efficiency", "Quad efficiency", "percent", 100.0, True),
)


@dataclass(frozen=True)
class SceneConfig:
    slug: str
    title: str
    timing_dir: Path
    raster_dir: Path


SCENES = (
    SceneConfig(
        "sponza",
        "Sponza",
        RESULTS_DIR / "ex15_donut_sponza_compare_fixed",
        RESULTS_DIR / "ex16_raster_stats_sponza",
    ),
    SceneConfig(
        "sponza_ivy",
        "Sponza + Ivy",
        RESULTS_DIR / "ex14_donut_sponza_ivy_compare",
        RESULTS_DIR / "ex17_raster_stats_sponza_ivy",
    ),
)


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        return list(csv.DictReader(handle))


def one_match(directory: Path, pattern: str) -> Path:
    matches = sorted(directory.glob(pattern))
    if len(matches) != 1:
        raise RuntimeError(
            f"Expected exactly one file for {directory / pattern}, "
            f"found {len(matches)}"
        )
    return matches[0]


def profile_path(timing_dir: Path, run_id: int) -> Path:
    run_dir = one_match(timing_dir, "*_runs")
    return one_match(run_dir, f"run_{run_id:05d}.csv_{run_id}_result.csv")


def raster_path(raster_dir: Path) -> Path:
    run_dir = one_match(raster_dir, "*_runs")
    return one_match(run_dir, "run_00000_0_raster_stats.csv")


def load_profiles(
    config: SceneConfig,
) -> tuple[np.ndarray, dict[str, dict[str, np.ndarray]], np.ndarray]:
    frame_reference: np.ndarray | None = None
    index_reference: np.ndarray | None = None
    result: dict[str, dict[str, np.ndarray]] = {}

    for renderer_name, run_id, _ in RENDERERS:
        rows = read_csv(profile_path(config.timing_dir, run_id))
        if len(rows) != MEASURE_FRAMES // WINDOW_FRAMES:
            raise RuntimeError(
                f"{config.title}/{renderer_name}: expected 250 profile rows, "
                f"found {len(rows)}"
            )

        frames = np.asarray([float(row["frame"]) for row in rows])
        index_count = np.asarray([float(row["index_count"]) for row in rows])
        if frame_reference is None:
            frame_reference = frames
            index_reference = index_count
        else:
            if not np.array_equal(frames, frame_reference):
                raise RuntimeError(
                    f"{config.title}: renderer profile frame axes differ"
                )
            if not np.allclose(index_count, index_reference, atol=1e-4):
                raise RuntimeError(
                    f"{config.title}: renderer index-count profiles differ"
                )

        excluded = {"frame", "index_count"}
        series: dict[str, np.ndarray] = {}
        for key in rows[0]:
            if key in excluded or not key:
                continue
            series[key] = np.asarray([float(row[key]) for row in rows])
        if "total" not in series:
            raise RuntimeError(
                f"{config.title}/{renderer_name}: total timing is missing"
            )
        result[renderer_name] = series

    assert frame_reference is not None
    assert index_reference is not None
    return frame_reference, result, index_reference


def load_raster_windows(
    config: SceneConfig,
) -> tuple[dict[str, np.ndarray], np.ndarray]:
    rows = read_csv(raster_path(config.raster_dir))
    if len(rows) != MEASURE_FRAMES:
        raise RuntimeError(
            f"{config.title}: expected {MEASURE_FRAMES} raster rows, "
            f"found {len(rows)}"
        )

    frames = np.asarray([int(row["frame"]) for row in rows])
    expected_frames = np.arange(
        WARMUP_FRAMES,
        WARMUP_FRAMES + MEASURE_FRAMES,
    )
    if not np.array_equal(frames, expected_frames):
        raise RuntimeError(
            f"{config.title}: raster frames are not continuous "
            f"{WARMUP_FRAMES}..{WARMUP_FRAMES + MEASURE_FRAMES - 1}"
        )

    windows: dict[str, np.ndarray] = {}
    for metric in METRICS:
        values = np.asarray([float(row[metric.key]) for row in rows])
        values = values.reshape(-1, WINDOW_FRAMES).mean(axis=1)
        windows[metric.key] = values * metric.scale

    triangle_count = np.asarray(
        [float(row["triangle_count"]) for row in rows]
    )
    triangle_windows = triangle_count.reshape(-1, WINDOW_FRAMES).mean(axis=1)
    return windows, triangle_windows * 3.0


def pearson(lhs: np.ndarray, rhs: np.ndarray) -> float:
    finite = np.isfinite(lhs) & np.isfinite(rhs)
    if finite.sum() < 2:
        return math.nan
    lhs_finite = lhs[finite]
    rhs_finite = rhs[finite]
    if np.ptp(lhs_finite) == 0.0 or np.ptp(rhs_finite) == 0.0:
        return math.nan
    return float(np.corrcoef(lhs_finite, rhs_finite)[0, 1])


def metric_formatter(metric: Metric):
    if metric.percent:
        return PercentFormatter(xmax=100.0, decimals=0)

    if metric.key in {"avg_overdraw", "max_overdraw"}:
        return FuncFormatter(lambda value, _: f"{value:.1f}")

    def compact(value: float, _: int) -> str:
        magnitude = abs(value)
        if magnitude >= 1_000_000:
            return f"{value / 1_000_000:.1f}M"
        if magnitude >= 1_000:
            return f"{value / 1_000:.0f}K"
        return f"{value:.0f}"

    return FuncFormatter(compact)


def padded_limits(values: np.ndarray) -> tuple[float, float]:
    finite = values[np.isfinite(values)]
    if finite.size == 0:
        return 0.0, 1.0
    low = float(finite.min())
    high = float(finite.max())
    if low == high:
        padding = max(abs(low) * 0.05, 1.0)
    else:
        padding = (high - low) * 0.06
    return max(0.0, low - padding), high + padding


def style_time_axis(axis: plt.Axes, title: str) -> None:
    axis.set_ylabel("GPU time (ms)")
    axis.set_title(title, loc="left", fontsize=11, fontweight="bold")
    axis.grid(True, color="#d1d5db", linewidth=0.7, alpha=0.7)
    axis.set_axisbelow(True)


def plot_metric(
    output_path: Path,
    config: SceneConfig,
    metric: Metric,
    frames: np.ndarray,
    profiles: dict[str, dict[str, np.ndarray]],
    metric_values: np.ndarray,
) -> None:
    fig, (total_axis, pass_axis) = plt.subplots(
        2,
        1,
        figsize=(16, 10),
        sharex=True,
        gridspec_kw={"height_ratios": [1.05, 1.0]},
    )
    fig.subplots_adjust(
        left=0.075,
        right=0.91,
        top=0.885,
        bottom=0.08,
        hspace=0.23,
    )

    metric_color = "#7e22ce"
    metric_limits = padded_limits(metric_values)

    total_lines = []
    total_labels = []
    for renderer_name, _, color in RENDERERS:
        line, = total_axis.plot(
            frames,
            profiles[renderer_name]["total"],
            color=color,
            linewidth=2.2,
            label=f"{renderer_name} total",
        )
        total_lines.append(line)
        total_labels.append(line.get_label())

    total_metric_axis = total_axis.twinx()
    metric_line, = total_metric_axis.plot(
        frames,
        metric_values,
        color=metric_color,
        linewidth=2.4,
        alpha=0.9,
        label=f"Metric: {metric.label}",
    )
    total_metric_axis.set_ylim(metric_limits)
    total_metric_axis.set_ylabel(
        f"{metric.label} ({metric.unit})",
        color=metric_color,
    )
    total_metric_axis.tick_params(axis="y", colors=metric_color)
    total_metric_axis.yaxis.set_major_formatter(metric_formatter(metric))

    style_time_axis(
        total_axis,
        "Renderer totals + raster metric (four primary lines)",
    )
    total_axis.legend(
        total_lines,
        total_labels,
        loc="upper left",
        ncol=3,
        frameon=True,
        fontsize=9,
    )
    total_metric_axis.legend(
        [metric_line],
        [metric_line.get_label()],
        loc="upper right",
        frameon=True,
        fontsize=9,
    )

    correlation_text = []
    for renderer_name, _, _ in RENDERERS:
        value = pearson(profiles[renderer_name]["total"], metric_values)
        correlation_text.append(f"{renderer_name} r={value:+.3f}")
    total_axis.text(
        0.01,
        0.025,
        "Pearson correlation: " + " | ".join(correlation_text),
        transform=total_axis.transAxes,
        fontsize=8.5,
        color="#374151",
        bbox={
            "boxstyle": "round,pad=0.3",
            "facecolor": "white",
            "edgecolor": "#d1d5db",
            "alpha": 0.88,
        },
    )

    pass_lines = []
    pass_labels = []
    for renderer_name, _, color in RENDERERS:
        for pass_name, values in profiles[renderer_name].items():
            if pass_name == "total":
                continue
            line, = pass_axis.plot(
                frames,
                values,
                color=color,
                linestyle=PASS_STYLES.get(pass_name, "--"),
                linewidth=1.35,
                alpha=0.88,
                label=f"{renderer_name} / {pass_name}",
            )
            pass_lines.append(line)
            pass_labels.append(line.get_label())

    pass_metric_axis = pass_axis.twinx()
    pass_metric_axis.plot(
        frames,
        metric_values,
        color=metric_color,
        linewidth=1.9,
        alpha=0.62,
    )
    pass_metric_axis.set_ylim(metric_limits)
    pass_metric_axis.set_ylabel(
        f"{metric.label} ({metric.unit})",
        color=metric_color,
    )
    pass_metric_axis.tick_params(axis="y", colors=metric_color)
    pass_metric_axis.yaxis.set_major_formatter(metric_formatter(metric))

    style_time_axis(
        pass_axis,
        "Individual GPU passes (metric repeated on the right axis)",
    )
    pass_axis.legend(
        pass_lines,
        pass_labels,
        loc="upper left",
        ncol=3,
        frameon=True,
        fontsize=7.8,
    )
    pass_axis.set_xlabel("Measured camera frame (10-frame windows)")
    pass_axis.set_xlim(float(frames.min()), float(frames.max()))

    fig.suptitle(
        f"{config.title} — {metric.label} vs Donut renderer timing",
        fontsize=17,
        fontweight="bold",
        y=0.97,
    )
    fig.text(
        0.5,
        0.925,
        (
            f"{GPU_LABEL}  |  1920×1080  |  playback  |  VFC on  |  "
            f"{MEASURE_FRAMES} measured frames  |  values are "
            f"{WINDOW_FRAMES}-frame means"
        ),
        ha="center",
        fontsize=10,
        color="#374151",
    )
    fig.savefig(output_path, dpi=180, facecolor="white")
    plt.close(fig)


def correlation_rows(
    scene: str,
    metric: Metric,
    profiles: dict[str, dict[str, np.ndarray]],
    metric_values: np.ndarray,
) -> Iterable[dict[str, object]]:
    for renderer_name, _, _ in RENDERERS:
        for pass_name, values in profiles[renderer_name].items():
            yield {
                "scene": scene,
                "metric": metric.key,
                "metric_label": metric.label,
                "renderer": renderer_name,
                "series": pass_name,
                "series_kind": "total" if pass_name == "total" else "pass",
                "pearson_r": pearson(values, metric_values),
            }


def write_csv(
    path: Path,
    rows: list[dict[str, object]],
    fieldnames: list[str],
) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def plot_correlation_overview(
    output_path: Path,
    config: SceneConfig,
    correlations: list[dict[str, object]],
) -> None:
    values = np.empty((len(METRICS), len(RENDERERS)))
    for metric_index, metric in enumerate(METRICS):
        for renderer_index, (renderer_name, _, _) in enumerate(RENDERERS):
            match = next(
                row
                for row in correlations
                if row["scene"] == config.slug
                and row["metric"] == metric.key
                and row["renderer"] == renderer_name
                and row["series"] == "total"
            )
            values[metric_index, renderer_index] = float(match["pearson_r"])

    fig, axis = plt.subplots(figsize=(10, 8.5))
    image = axis.imshow(values, cmap="RdBu_r", vmin=-1.0, vmax=1.0)
    axis.set_xticks(
        np.arange(len(RENDERERS)),
        [renderer[0] for renderer in RENDERERS],
    )
    axis.set_yticks(
        np.arange(len(METRICS)),
        [metric.label for metric in METRICS],
    )
    for row_index in range(values.shape[0]):
        for column_index in range(values.shape[1]):
            value = values[row_index, column_index]
            color = "white" if abs(value) >= 0.55 else "#111827"
            axis.text(
                column_index,
                row_index,
                f"{value:+.2f}",
                ha="center",
                va="center",
                color=color,
                fontsize=9,
            )
    axis.set_title(
        f"{config.title} — raster metric vs renderer total correlation\n"
        f"{GPU_LABEL} | Pearson r over 250 aligned windows",
        fontsize=14,
        fontweight="bold",
        pad=16,
    )
    colorbar = fig.colorbar(image, ax=axis, shrink=0.88)
    colorbar.set_label("Pearson correlation (r)")
    fig.tight_layout()
    fig.savefig(output_path, dpi=180, facecolor="white")
    plt.close(fig)


def write_readme(
    output_dir: Path,
    validation_rows: list[dict[str, object]],
) -> None:
    validation_lines = "\n".join(
        (
            f"- {row['scene']}: {row['profile_windows']} timing windows, "
            f"{row['raster_frames']} raster frames, max |index difference| "
            f"= {float(row['max_abs_index_difference']):.6f}"
        )
        for row in validation_rows
    )
    metric_lines = "\n".join(
        f"- `{metric.key}`: {metric.label} ({metric.unit})"
        for metric in METRICS
    )
    content = f"""# Sponza raster-metric comparison

Hardware: **{GPU_LABEL}**

Each scene folder contains one two-panel plot per raster metric:

- top: Deferred total, Prepass total, VisBuf total, and the metric;
- bottom: every renderer pass, with the same metric repeated on the right axis.

All curves use the same playback camera, 1920×1080, VFC enabled, 60 warm-up
frames, {MEASURE_FRAMES} measured frames, and {WINDOW_FRAMES}-frame mean
windows. The raster renderer uses the same Donut Assimp scene hierarchy
converted to benchmark buffers, so its VFC workload matches the timed Donut
renderers exactly.

Workload validation:

{validation_lines}

Metrics:

{metric_lines}

`correlations.csv` contains Pearson correlations for every total and pass.
`workload_validation.csv` records the frame/index alignment check.

Important: raster metrics come from the project's compute-based software
raster-stat pass. They describe the benchmark raster model and are not native
hardware performance counters.
"""
    (output_dir / "README.md").write_text(content, encoding="utf-8")


def archive_output(output_dir: Path) -> Path:
    archive_base = output_dir.parent / output_dir.name
    archive_path = Path(
        shutil.make_archive(
            str(archive_base),
            "zip",
            root_dir=output_dir.parent,
            base_dir=output_dir.name,
        )
    )
    return archive_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot Sponza Donut timings against raster statistics."
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help=f"Output directory (default: {DEFAULT_OUTPUT_DIR})",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    all_correlations: list[dict[str, object]] = []
    validation_rows: list[dict[str, object]] = []
    metric_summary_rows: list[dict[str, object]] = []

    for config in SCENES:
        scene_output = output_dir / config.slug
        scene_output.mkdir(parents=True, exist_ok=True)

        frames, profiles, profile_indices = load_profiles(config)
        raster_windows, raster_indices = load_raster_windows(config)
        index_difference = np.abs(profile_indices - raster_indices)
        validation_rows.append(
            {
                "scene": config.slug,
                "profile_windows": len(frames),
                "raster_frames": MEASURE_FRAMES,
                "max_abs_index_difference": float(index_difference.max()),
                "mean_abs_index_difference": float(index_difference.mean()),
                "exact_window_count": int((index_difference < 1e-3).sum()),
                "window_count": len(frames),
                "index_correlation": pearson(
                    profile_indices,
                    raster_indices,
                ),
            }
        )
        if not np.allclose(profile_indices, raster_indices, atol=1e-3):
            raise RuntimeError(
                f"{config.title}: raster and Donut index workloads do not match"
            )

        scene_correlations: list[dict[str, object]] = []
        for metric in METRICS:
            metric_values = raster_windows[metric.key]
            plot_metric(
                scene_output / f"{metric.key}_vs_timing.png",
                config,
                metric,
                frames,
                profiles,
                metric_values,
            )
            rows = list(
                correlation_rows(
                    config.slug,
                    metric,
                    profiles,
                    metric_values,
                )
            )
            scene_correlations.extend(rows)
            all_correlations.extend(rows)
            metric_summary_rows.append(
                {
                    "scene": config.slug,
                    "metric": metric.key,
                    "metric_label": metric.label,
                    "unit": metric.unit,
                    "minimum": float(metric_values.min()),
                    "mean": float(metric_values.mean()),
                    "maximum": float(metric_values.max()),
                }
            )

        plot_correlation_overview(
            scene_output / "total_correlation_overview.png",
            config,
            scene_correlations,
        )

    write_csv(
        output_dir / "correlations.csv",
        all_correlations,
        [
            "scene",
            "metric",
            "metric_label",
            "renderer",
            "series",
            "series_kind",
            "pearson_r",
        ],
    )
    write_csv(
        output_dir / "metric_summary.csv",
        metric_summary_rows,
        [
            "scene",
            "metric",
            "metric_label",
            "unit",
            "minimum",
            "mean",
            "maximum",
        ],
    )
    write_csv(
        output_dir / "workload_validation.csv",
        validation_rows,
        [
            "scene",
            "profile_windows",
            "raster_frames",
            "max_abs_index_difference",
            "mean_abs_index_difference",
            "exact_window_count",
            "window_count",
            "index_correlation",
        ],
    )
    write_readme(output_dir, validation_rows)
    archive_path = archive_output(output_dir)

    print(f"Created {len(SCENES) * len(METRICS)} metric plots")
    print(f"Output: {output_dir}")
    print(f"Archive: {archive_path}")


if __name__ == "__main__":
    main()
