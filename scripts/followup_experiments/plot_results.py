#!/usr/bin/env python3
"""Create the post-fairness plots and plot-ready data tables.

The structure deliberately reuses the successful early experiment approach:

* pass-by-frame and median pass breakdown from ex10
* aligned software-raster/profile windows and correlation plots from ex10 and
  ``datas/experiments/succeed/ex12-18 scripts``
* direct sampled heatmaps, winner boundaries, and speedup ratios from ex5/ex6

Only ``runner_status == success`` rows are treated as measurements.
"""

from __future__ import annotations

import csv
import json
import math
import shutil
from collections import defaultdict
from pathlib import Path
from typing import Iterable, Sequence

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib.colors import TwoSlopeNorm


ROOT = Path(__file__).resolve().parent
RESULTS = ROOT / "results"
PLOTS = ROOT / "plots"
PNG = PLOTS / "png"
SVG = PLOTS / "svg"
DATA = PLOTS / "data"
MANIFEST = RESULTS / "_campaign_manifest.json"

HARDWARE = "NVIDIA GeForce RTX 5060 Ti 16GB"
ARCHIVE_HARDWARE = "Earlier datas/ archive: NVIDIA GeForce RTX 5070"
FOOTER = (
    f"Measured on {HARDWARE}. {ARCHIVE_HARDWARE}; "
    "archive values are not pooled with this campaign."
)

RENDERER_NAMES = {
    8: "DeferredPrepass",
    9: "VisBuf",
    10: "SoftwareRasterStats",
}
RENDERER_COLORS = {
    "DeferredPrepass": "#d95f02",
    "VisBuf": "#1b9e77",
    "SoftwareRasterStats": "#7570b3",
}
PASS_ORDER = [
    "depth_prepass",
    "visibility",
    "geometry",
    "visutil_histogram",
    "visutil_prefix",
    "visutil_flatten",
    "gbuffer",
    "lighting",
    "tonemap",
    "raster_stats",
]
PASS_COLORS = {
    "depth_prepass": "#9ecae1",
    "visibility": "#3182bd",
    "geometry": "#31a354",
    "visutil_histogram": "#fdae6b",
    "visutil_prefix": "#fd8d3c",
    "visutil_flatten": "#e6550d",
    "gbuffer": "#756bb1",
    "lighting": "#636363",
    "tonemap": "#bdbdbd",
    "raster_stats": "#969696",
}
PASS_LABELS = {
    "depth_prepass": "Depth pre-pass",
    "visibility": "Visibility",
    "geometry": "G-buffer raster",
    "visutil_histogram": "Histogram",
    "visutil_prefix": "Prefix",
    "visutil_flatten": "Flatten/reorder",
    "gbuffer": "Compute G-buffer",
    "lighting": "Lighting",
    "tonemap": "Tonemap",
    "raster_stats": "Software raster stats",
}
SCENE_ORDER = ["Sponza", "Sponza Ivy", "Bistro"]
PLOT_INDEX: list[dict[str, str]] = []


def scene_label(value: object) -> str:
    text = str(value).lower()
    if "bistro" in text:
        return "Bistro"
    if "ivy" in text:
        return "Sponza Ivy"
    if "sponza" in text:
        return "Sponza"
    return "Synthetic"


def clean_output() -> None:
    resolved = PLOTS.resolve()
    if resolved.parent != ROOT.resolve():
        raise RuntimeError(f"Refusing to clean unexpected plot path: {resolved}")
    if resolved.exists():
        shutil.rmtree(resolved)
    for path in (PNG, SVG, DATA):
        path.mkdir(parents=True, exist_ok=True)


def configure_style() -> None:
    plt.rcParams.update(
        {
            "figure.dpi": 120,
            "savefig.dpi": 180,
            "svg.hashsalt": "VisibilityBufferInfo-followup-20260730",
            "font.size": 10,
            "axes.titlesize": 12,
            "axes.labelsize": 10,
            "axes.grid": True,
            "grid.alpha": 0.22,
            "legend.frameon": False,
            "axes.spines.top": False,
            "axes.spines.right": False,
        }
    )


def save_figure(
    fig: plt.Figure,
    name: str,
    description: str,
    inputs: Iterable[str],
    filters: str = "runner_status=success",
    layout_top: float = 1.0,
) -> None:
    fig.text(0.5, 0.005, FOOTER, ha="center", va="bottom", fontsize=8)
    fig.tight_layout(rect=(0, 0.035, 1, layout_top))
    png_path = PNG / f"{name}.png"
    svg_path = SVG / f"{name}.svg"
    fig.savefig(
        png_path,
        bbox_inches="tight",
        metadata={"Software": "VisibilityBufferInfo plot_results.py"},
    )
    fig.savefig(
        svg_path,
        bbox_inches="tight",
        metadata={
            "Creator": "VisibilityBufferInfo plot_results.py",
            "Date": "2026-07-30",
        },
    )
    svg_text = svg_path.read_text(encoding="utf-8")
    svg_path.write_text(
        "\n".join(line.rstrip() for line in svg_text.splitlines()) + "\n",
        encoding="utf-8",
    )
    plt.close(fig)
    PLOT_INDEX.append(
        {
            "plot": name,
            "png": png_path.relative_to(PLOTS).as_posix(),
            "svg": svg_path.relative_to(PLOTS).as_posix(),
            "description": description,
            "inputs": "; ".join(inputs),
            "filters": filters,
            "hardware": HARDWARE,
        }
    )


def numeric(data: pd.DataFrame, columns: Iterable[str]) -> None:
    for column in columns:
        if column in data:
            data[column] = pd.to_numeric(data[column], errors="coerce")


def load_config(config: str) -> pd.DataFrame:
    path = RESULTS / config / f"{config}.csv"
    # The runner CSV has many single-column blocks. Consolidate them before
    # adding analysis columns so repeated plotting stays quiet and efficient.
    data = pd.read_csv(path, encoding="utf-8-sig").copy()
    data["source_config"] = config
    data["renderer_variant_numeric"] = pd.to_numeric(
        data["param_renderer_variant"], errors="coerce"
    )
    data["renderer"] = data["renderer_variant_numeric"].map(RENDERER_NAMES)
    data["scene"] = data["param_scene_path"].map(scene_label)
    data["linear_gbuffer"] = (
        pd.to_numeric(data["param_donut_linear_gbuffer"], errors="coerce")
        .fillna(0)
        .astype(int)
        .astype(bool)
    )
    numeric(
        data,
        [
            "total_time_min_ms",
            "total_time_median_ms",
            "total_time_max_ms",
            "total_time_avg_ms",
            "total_time_p01_ms",
            "total_time_p10_ms",
            "total_time_p90_ms",
            "total_time_p99_ms",
            "param_material_count",
            "param_material_assign_max_open",
            "param_material_assign_locality",
            "param_material_assign_diversity",
            "param_geometry_div",
            "param_window_width",
            "param_window_height",
            "param_seed",
            "param_to_load_texture",
            "param_use_vfc",
        ],
    )
    return data[data["runner_status"] == "success"].copy()


def load_all_success() -> pd.DataFrame:
    frames = []
    for result_dir in sorted(RESULTS.glob("[0-9][0-9]_*")):
        csv_path = result_dir / f"{result_dir.name}.csv"
        if csv_path.exists():
            frames.append(load_config(result_dir.name))
    return pd.concat(frames, ignore_index=True, sort=False)


def build_pass_long(all_rows: pd.DataFrame) -> pd.DataFrame:
    rows: list[dict[str, object]] = []
    for _, source in all_rows.iterrows():
        for slot in range(32):
            name = str(source.get(f"pass_name_{slot}", "") or "").strip()
            value = pd.to_numeric(
                pd.Series([source.get(f"pass_{slot}_time_avg_ms")]),
                errors="coerce",
            ).iloc[0]
            if not name or name == "nan" or not math.isfinite(float(value)):
                continue
            rows.append(
                {
                    "source_config": source["source_config"],
                    "runner_run_index": int(source["runner_run_index"]),
                    "renderer": source["renderer"],
                    "scene": source["scene"],
                    "linear_gbuffer": bool(source["linear_gbuffer"]),
                    "pass_slot": slot,
                    "pass_name": name,
                    "pass_time_avg_ms": float(value),
                }
            )
    return pd.DataFrame(rows)


def mean_error(
    data: pd.DataFrame,
    groups: Sequence[str],
    value: str = "total_time_avg_ms",
) -> pd.DataFrame:
    result = (
        data.groupby(list(groups), dropna=False)[value]
        .agg(["mean", "std", "count"])
        .reset_index()
    )
    result["sem"] = result["std"].fillna(0) / np.sqrt(result["count"])
    return result


def paired_speedup(
    data: pd.DataFrame,
    groups: Sequence[str],
    value: str = "total_time_avg_ms",
) -> pd.DataFrame:
    grouped = data.groupby(list(groups) + ["renderer"], dropna=False)[value].mean()
    wide = grouped.unstack("renderer").reset_index()
    wide["deferred_over_visbuf"] = (
        wide["DeferredPrepass"] / wide["VisBuf"]
    )
    wide["visbuf_percent_faster"] = (
        wide["deferred_over_visbuf"] - 1.0
    ) * 100.0
    return wide


def renderer_response_plot(
    data: pd.DataFrame,
    x: str,
    title: str,
    name: str,
    x_label: str,
    input_name: str,
    groups: Sequence[str] = (),
    group_label: str | None = None,
    log_x: bool = False,
) -> pd.DataFrame:
    summary = mean_error(data, [x, *groups, "renderer"])
    pair_groups = [x, *groups]
    speed = paired_speedup(data, pair_groups)

    fig, axes = plt.subplots(1, 2, figsize=(14, 5.4))
    group_values = [()] if not groups else list(
        summary[list(groups)].drop_duplicates().itertuples(index=False, name=None)
    )
    line_styles = ["-", "--", ":", "-."]
    for group_index, group_values_tuple in enumerate(group_values):
        selector = np.ones(len(summary), dtype=bool)
        speed_selector = np.ones(len(speed), dtype=bool)
        suffix_parts = []
        for column, value in zip(groups, group_values_tuple):
            selector &= summary[column] == value
            speed_selector &= speed[column] == value
            suffix_parts.append(f"{column}={value:g}" if isinstance(value, float) else f"{column}={value}")
        suffix = ", ".join(suffix_parts)
        for renderer in ("DeferredPrepass", "VisBuf"):
            part = summary[selector & (summary["renderer"] == renderer)].sort_values(x)
            label = renderer if not suffix else f"{renderer} · {suffix}"
            axes[0].errorbar(
                part[x],
                part["mean"],
                yerr=part["sem"],
                marker="o",
                markersize=4,
                linewidth=1.8,
                linestyle=line_styles[group_index % len(line_styles)],
                color=RENDERER_COLORS[renderer],
                alpha=max(0.45, 1.0 - group_index * 0.12),
                label=label,
            )
        part_speed = speed[speed_selector].sort_values(x)
        label = suffix or "Deferred / VisBuf"
        axes[1].plot(
            part_speed[x],
            part_speed["deferred_over_visbuf"],
            marker="o",
            linewidth=1.8,
            linestyle=line_styles[group_index % len(line_styles)],
            label=label,
        )
    axes[0].set_title("Measured total GPU time")
    axes[0].set_xlabel(x_label)
    axes[0].set_ylabel("Total GPU time (ms)")
    axes[0].legend(fontsize=8, ncol=2)
    axes[1].axhline(1.0, color="black", linewidth=1)
    axes[1].set_title("Speed ratio")
    axes[1].set_xlabel(x_label)
    axes[1].set_ylabel("DeferredPrepass / VisBuf (>1 favors VisBuf)")
    if groups:
        axes[1].legend(title=group_label, fontsize=8)
    if log_x:
        axes[0].set_xscale("log", base=2)
        axes[1].set_xscale("log", base=2)
    fig.suptitle(title)
    summary.to_csv(DATA / f"{name}_time_summary.csv", index=False)
    speed.to_csv(DATA / f"{name}_paired_speedup.csv", index=False)
    save_figure(
        fig,
        name,
        title,
        [f"results/{input_name}/{input_name}.csv"],
        "success rows; points are mean; error bars are SEM",
    )
    return speed


def plot_material_and_class_responses() -> None:
    material = load_config("02_synth_material_count_same_class_dense")
    renderer_response_plot(
        material,
        "param_material_count",
        "Synthetic material count with one shared generic class",
        "01_synth_material_count_same_class",
        "Material count",
        "02_synth_material_count_same_class_dense",
        log_x=True,
    )

    classes = load_config("03_synth_class_count_fixed_materials_dense")
    renderer_response_plot(
        classes,
        "param_material_assign_max_open",
        "Synthetic class / generic PSO count with 255 materials",
        "02_synth_class_count_fixed_materials",
        "Material class count",
        "03_synth_class_count_fixed_materials_dense",
        log_x=True,
    )


def plot_locality_and_diversity() -> None:
    locality = load_config("04_synth_locality_dense")
    renderer_response_plot(
        locality,
        "param_material_assign_locality",
        "Synthetic locality response (255 materials, 64 classes)",
        "03_synth_locality_response",
        "Locality",
        "04_synth_locality_dense",
    )

    diversity = load_config("05_synth_diversity_dense")
    summary = mean_error(
        diversity,
        ["param_material_assign_locality", "param_material_assign_diversity", "renderer"],
    )
    speed = paired_speedup(
        diversity,
        ["param_material_assign_locality", "param_material_assign_diversity", "param_seed"],
    )
    speed_summary = mean_error(
        speed.rename(columns={"deferred_over_visbuf": "ratio"}),
        ["param_material_assign_locality", "param_material_assign_diversity"],
        "ratio",
    )
    fig, axes = plt.subplots(2, 2, figsize=(14, 9), sharex=True)
    for column_index, locality_value in enumerate((0.0, 1.0)):
        axis = axes[0, column_index]
        for renderer in ("DeferredPrepass", "VisBuf"):
            part = summary[
                (summary["param_material_assign_locality"] == locality_value)
                & (summary["renderer"] == renderer)
            ].sort_values("param_material_assign_diversity")
            axis.errorbar(
                part["param_material_assign_diversity"],
                part["mean"],
                yerr=part["sem"],
                marker="o",
                color=RENDERER_COLORS[renderer],
                label=renderer,
            )
        axis.set_title(f"Locality={locality_value:g}: total time")
        axis.set_ylabel("GPU time (ms)")
        axis.legend()
        speed_part = speed_summary[
            speed_summary["param_material_assign_locality"] == locality_value
        ].sort_values("param_material_assign_diversity")
        axes[1, column_index].errorbar(
            speed_part["param_material_assign_diversity"],
            speed_part["mean"],
            yerr=speed_part["sem"],
            marker="o",
            color="#377eb8",
        )
        axes[1, column_index].axhline(1.0, color="black", linewidth=1)
        axes[1, column_index].set_title("DeferredPrepass / VisBuf")
        axes[1, column_index].set_xlabel("Diversity")
        axes[1, column_index].set_ylabel("Ratio (>1 favors VisBuf)")
    fig.suptitle("Synthetic diversity response at both locality extremes")
    summary.to_csv(DATA / "04_synth_diversity_time_summary.csv", index=False)
    speed.to_csv(DATA / "04_synth_diversity_paired_speedup.csv", index=False)
    save_figure(
        fig,
        "04_synth_diversity_response",
        "Diversity response at locality 0 and 1",
        ["results/05_synth_diversity_dense/05_synth_diversity_dense.csv"],
        "success rows; three seed pairs",
    )


def plot_phase_map() -> None:
    data = load_config("06_synth_locality_diversity_phase_dense")
    speed = paired_speedup(
        data,
        [
            "param_material_assign_locality",
            "param_material_assign_diversity",
            "param_seed",
        ],
    )
    grid_data = (
        speed.groupby(
            ["param_material_assign_locality", "param_material_assign_diversity"]
        )["deferred_over_visbuf"]
        .mean()
        .reset_index()
    )
    localities = sorted(grid_data["param_material_assign_locality"].unique())
    diversities = sorted(grid_data["param_material_assign_diversity"].unique())
    grid = (
        grid_data.pivot(
            index="param_material_assign_diversity",
            columns="param_material_assign_locality",
            values="deferred_over_visbuf",
        )
        .reindex(index=diversities, columns=localities)
        .to_numpy()
    )
    fig, axis = plt.subplots(figsize=(8.5, 7))
    vmax = max(abs(float(np.nanmin(grid)) - 1), abs(float(np.nanmax(grid)) - 1))
    image = axis.imshow(
        grid,
        origin="lower",
        aspect="auto",
        extent=(min(localities), max(localities), min(diversities), max(diversities)),
        cmap="RdBu_r",
        norm=TwoSlopeNorm(vcenter=1.0, vmin=1.0 - vmax, vmax=1.0 + vmax),
    )
    if float(np.nanmin(grid)) <= 1.0 <= float(np.nanmax(grid)):
        x_mesh, y_mesh = np.meshgrid(localities, diversities)
        axis.contour(x_mesh, y_mesh, grid, levels=[1.0], colors="black", linewidths=2)
    colorbar = fig.colorbar(image, ax=axis)
    colorbar.set_label("DeferredPrepass / VisBuf")
    axis.set_xlabel("Locality")
    axis.set_ylabel("Diversity")
    axis.set_title(
        "Synthetic locality × diversity crossover map\n"
        "Red: VisBuf advantage · Blue: DeferredPrepass advantage"
    )
    grid_data.to_csv(DATA / "05_synth_phase_speedup_grid.csv", index=False)
    save_figure(
        fig,
        "05_synth_locality_diversity_phase",
        "Direct sampled 9×9 phase map with equal-time contour",
        [
            "results/06_synth_locality_diversity_phase_dense/"
            "06_synth_locality_diversity_phase_dense.csv"
        ],
        "success rows; ratio averaged over two paired seeds",
    )


def plot_material_class_matrix() -> None:
    data = load_config("07_synth_material_class_matrix")
    speed = paired_speedup(
        data,
        ["param_material_count", "param_material_assign_max_open", "param_seed"],
    )
    mean_speed = (
        speed.groupby(["param_material_count", "param_material_assign_max_open"])[
            "deferred_over_visbuf"
        ]
        .mean()
        .reset_index()
    )
    materials = sorted(mean_speed["param_material_count"].unique())
    classes = sorted(mean_speed["param_material_assign_max_open"].unique())
    matrix = (
        mean_speed.pivot(
            index="param_material_count",
            columns="param_material_assign_max_open",
            values="deferred_over_visbuf",
        )
        .reindex(index=materials, columns=classes)
        .to_numpy()
    )
    fig, axis = plt.subplots(figsize=(11, 6))
    valid = matrix[np.isfinite(matrix)]
    vmax = max(abs(float(valid.min()) - 1), abs(float(valid.max()) - 1))
    image = axis.imshow(
        matrix,
        origin="lower",
        aspect="auto",
        cmap="RdBu_r",
        norm=TwoSlopeNorm(vcenter=1.0, vmin=1.0 - vmax, vmax=1.0 + vmax),
    )
    axis.set_xticks(range(len(classes)), [str(int(value)) for value in classes], rotation=45)
    axis.set_yticks(range(len(materials)), [str(int(value)) for value in materials])
    axis.set_xlabel("Material class / generic PSO count")
    axis.set_ylabel("Material count")
    axis.set_title("Material count × class count: DeferredPrepass / VisBuf")
    colorbar = fig.colorbar(image, ax=axis)
    colorbar.set_label("Ratio (>1 favors VisBuf)")
    mean_speed.to_csv(DATA / "06_material_class_matrix_speedup.csv", index=False)
    save_figure(
        fig,
        "06_synth_material_class_matrix",
        "Independent material-count and class-count matrix",
        [
            "results/07_synth_material_class_matrix/"
            "07_synth_material_class_matrix.csv"
        ],
        "success rows; two paired seeds; impossible class>material cells are blank",
    )


def plot_scaling_and_seed() -> None:
    workload = load_config("08_synth_workload_scaling_dense")
    renderer_response_plot(
        workload,
        "param_geometry_div",
        "Synthetic workload scaling at three locality levels",
        "07_synth_workload_scaling",
        "Geometry division",
        "08_synth_workload_scaling_dense",
        groups=("param_material_assign_locality",),
        group_label="Locality",
        log_x=True,
    )

    resolution = load_config("09_synth_resolution_scaling_dense")
    resolution["megapixels"] = (
        resolution["param_window_width"] * resolution["param_window_height"] / 1e6
    )
    renderer_response_plot(
        resolution,
        "megapixels",
        "Synthetic resolution scaling at three locality levels",
        "08_synth_resolution_scaling",
        "Resolution (megapixels)",
        "09_synth_resolution_scaling_dense",
        groups=("param_material_assign_locality",),
        group_label="Locality",
    )

    seed = load_config("10_synth_seed_robustness_dense")
    paired = paired_speedup(seed, ["param_seed"])
    fig, axes = plt.subplots(1, 2, figsize=(13, 5))
    for renderer in ("DeferredPrepass", "VisBuf"):
        part = seed[seed["renderer"] == renderer].sort_values("param_seed")
        axes[0].plot(
            part["param_seed"],
            part["total_time_avg_ms"],
            marker="o",
            color=RENDERER_COLORS[renderer],
            label=renderer,
        )
    axes[0].set_xlabel("Seed")
    axes[0].set_ylabel("Total GPU time (ms)")
    axes[0].set_title("Per-seed totals")
    axes[0].legend()
    axes[1].hist(
        paired["deferred_over_visbuf"],
        bins=10,
        color="#377eb8",
        alpha=0.8,
    )
    axes[1].axvline(1.0, color="black", linewidth=1)
    axes[1].set_xlabel("DeferredPrepass / VisBuf")
    axes[1].set_ylabel("Seed count")
    axes[1].set_title(
        f"Speed ratio distribution\nmean={paired['deferred_over_visbuf'].mean():.3f}, "
        f"std={paired['deferred_over_visbuf'].std():.3f}"
    )
    fig.suptitle("Synthetic seed robustness (20 paired seeds)")
    paired.to_csv(DATA / "09_seed_robustness_pairs.csv", index=False)
    save_figure(
        fig,
        "09_synth_seed_robustness",
        "Per-seed paired results and speed-ratio distribution",
        [
            "results/10_synth_seed_robustness_dense/"
            "10_synth_seed_robustness_dense.csv"
        ],
        "success rows; 20 paired seeds",
    )


def plot_linear_control() -> None:
    data = load_config("11_synth_linear_gbuffer_control")
    data["condition"] = (
        "M"
        + data["param_material_count"].astype(int).astype(str)
        + "/C"
        + data["param_material_assign_max_open"].astype(int).astype(str)
        + "/L"
        + data["param_material_assign_locality"].map(lambda value: f"{value:g}")
    )
    speed = paired_speedup(
        data,
        ["condition", "linear_gbuffer", "param_seed"],
    )
    summary = (
        speed.groupby(["condition", "linear_gbuffer"])["deferred_over_visbuf"]
        .agg(["mean", "std"])
        .reset_index()
    )
    conditions = list(dict.fromkeys(data["condition"]))
    fig, axes = plt.subplots(1, 2, figsize=(15, 5.5), sharey=True)
    for axis, linear in zip(axes, (False, True)):
        part = summary[summary["linear_gbuffer"] == linear].set_index("condition").reindex(conditions)
        axis.bar(
            range(len(conditions)),
            part["mean"],
            yerr=part["std"].fillna(0),
            color="#377eb8" if linear else "#e41a1c",
            alpha=0.82,
        )
        axis.axhline(1.0, color="black", linewidth=1)
        axis.set_xticks(range(len(conditions)), conditions, rotation=40, ha="right")
        axis.set_title("Linear G-buffer control" if linear else "End-to-end sRGB ABI")
        axis.set_ylabel("DeferredPrepass / VisBuf")
    fig.suptitle("Synthetic sRGB path separation")
    summary.to_csv(DATA / "10_synth_linear_control_speedup.csv", index=False)
    save_figure(
        fig,
        "10_synth_srgb_vs_linear_control",
        "Synthetic end-to-end sRGB ABI versus linear G-buffer control",
        [
            "results/11_synth_linear_gbuffer_control/"
            "11_synth_linear_gbuffer_control.csv"
        ],
        "success rows; two paired seeds; error bars are seed std",
    )


def real_scene_response(
    config: str,
    x: str,
    x_label: str,
    name: str,
    title: str,
) -> None:
    data = load_config(config)
    summary = mean_error(data, ["scene", x, "renderer"])
    speed = paired_speedup(data, ["scene", x])
    fig, axes = plt.subplots(2, 3, figsize=(17, 8), sharex="col")
    for column, scene in enumerate(SCENE_ORDER):
        for renderer in ("DeferredPrepass", "VisBuf"):
            part = summary[
                (summary["scene"] == scene) & (summary["renderer"] == renderer)
            ].sort_values(x)
            axes[0, column].plot(
                part[x],
                part["mean"],
                marker="o",
                color=RENDERER_COLORS[renderer],
                label=renderer,
            )
        axes[0, column].set_title(scene)
        axes[0, column].set_ylabel("Total GPU time (ms)")
        axes[0, column].legend(fontsize=8)
        part_speed = speed[speed["scene"] == scene].sort_values(x)
        axes[1, column].plot(
            part_speed[x],
            part_speed["deferred_over_visbuf"],
            marker="o",
            color="#377eb8",
        )
        axes[1, column].axhline(1.0, color="black", linewidth=1)
        axes[1, column].set_xlabel(x_label)
        axes[1, column].set_ylabel("DeferredPrepass / VisBuf")
    fig.suptitle(title)
    summary.to_csv(DATA / f"{name}_time_summary.csv", index=False)
    speed.to_csv(DATA / f"{name}_speedup.csv", index=False)
    save_figure(
        fig,
        name,
        title,
        [f"results/{config}/{config}.csv"],
        "success rows; 600-frame camera prefix; no cross-GPU pooling",
    )


def plot_real_sweeps() -> None:
    real_scene_response(
        "12_real_class_count_dense",
        "param_material_assign_max_open",
        "Material class count",
        "11_real_scene_class_count",
        "Real-scene generic class / PSO count response",
    )
    real_scene_response(
        "13_real_diversity_dense",
        "param_material_assign_diversity",
        "Material-class diversity",
        "12_real_scene_diversity",
        "Real-scene material-class diversity response",
    )


def pass_breakdown_figure(data: pd.DataFrame, linear: bool) -> tuple[plt.Figure, pd.DataFrame]:
    selected = data[data["linear_gbuffer"] == linear]
    records = []
    for _, row in selected.iterrows():
        for slot in range(1, 32):
            pass_name = str(row.get(f"pass_name_{slot}", "") or "").strip()
            value = pd.to_numeric(
                pd.Series([row.get(f"pass_{slot}_time_avg_ms")]), errors="coerce"
            ).iloc[0]
            if pass_name and pass_name != "nan" and math.isfinite(float(value)):
                records.append(
                    {
                        "scene": row["scene"],
                        "renderer": row["renderer"],
                        "pass_name": pass_name,
                        "pass_time_avg_ms": float(value),
                    }
                )
    long = pd.DataFrame(records)
    summary = (
        long.groupby(["scene", "renderer", "pass_name"])["pass_time_avg_ms"]
        .mean()
        .reset_index()
    )
    bars = [(scene, renderer) for scene in SCENE_ORDER for renderer in ("DeferredPrepass", "VisBuf")]
    fig, axis = plt.subplots(figsize=(14, 6))
    bottom = np.zeros(len(bars))
    for pass_name in PASS_ORDER:
        values = []
        for scene, renderer in bars:
            match = summary[
                (summary["scene"] == scene)
                & (summary["renderer"] == renderer)
                & (summary["pass_name"] == pass_name)
            ]["pass_time_avg_ms"]
            values.append(float(match.iloc[0]) if not match.empty else 0.0)
        if np.allclose(values, 0):
            continue
        axis.bar(
            range(len(bars)),
            values,
            bottom=bottom,
            color=PASS_COLORS[pass_name],
            label=PASS_LABELS[pass_name],
        )
        bottom += np.asarray(values)
    axis.set_xticks(
        range(len(bars)),
        [f"{scene}\n{renderer}" for scene, renderer in bars],
        rotation=25,
        ha="right",
    )
    axis.set_ylabel("Average GPU time (ms)")
    axis.set_title(
        ("Linear G-buffer control" if linear else "End-to-end sRGB ABI")
        + " · pass order"
    )
    axis.legend(ncol=4, fontsize=8)
    return fig, summary


def plot_full_camera_summary() -> pd.DataFrame:
    data = load_config("14_real_full_camera_linear_control")
    stats = [
        ("total_time_avg_ms", "Average"),
        ("total_time_median_ms", "Median"),
        ("total_time_p90_ms", "P90"),
        ("total_time_p99_ms", "P99"),
    ]
    fig, axes = plt.subplots(2, 2, figsize=(15, 10))
    width = 0.19
    x = np.arange(len(SCENE_ORDER))
    for axis, (column, label) in zip(axes.ravel(), stats):
        for offset_index, (linear, renderer) in enumerate(
            [
                (False, "DeferredPrepass"),
                (False, "VisBuf"),
                (True, "DeferredPrepass"),
                (True, "VisBuf"),
            ]
        ):
            values = []
            for scene in SCENE_ORDER:
                row = data[
                    (data["scene"] == scene)
                    & (data["linear_gbuffer"] == linear)
                    & (data["renderer"] == renderer)
                ]
                values.append(float(row[column].iloc[0]))
            axis.bar(
                x + (offset_index - 1.5) * width,
                values,
                width,
                color=RENDERER_COLORS[renderer],
                alpha=1.0 if not linear else 0.5,
                hatch="" if not linear else "//",
                label=f"{renderer} · {'linear' if linear else 'sRGB'}",
            )
        axis.set_xticks(x, SCENE_ORDER)
        axis.set_ylabel("GPU time (ms)")
        axis.set_title(label)
    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.suptitle("Full-camera total-time statistics", y=0.99)
    fig.legend(
        handles,
        labels,
        loc="upper center",
        bbox_to_anchor=(0.5, 0.965),
        ncol=4,
    )
    summary_columns = [
        "scene",
        "renderer",
        "linear_gbuffer",
        "total_time_avg_ms",
        "total_time_median_ms",
        "total_time_p90_ms",
        "total_time_p99_ms",
    ]
    data[summary_columns].to_csv(DATA / "13_full_camera_total_statistics.csv", index=False)
    save_figure(
        fig,
        "13_full_camera_total_statistics",
        "Average, median, P90, and P99 full-camera totals",
        [
            "results/14_real_full_camera_linear_control/"
            "14_real_full_camera_linear_control.csv"
        ],
        "success rows; complete camera; sRGB and linear shown separately",
        layout_top=0.91,
    )

    for linear, suffix in ((False, "srgb"), (True, "linear")):
        figure, summary = pass_breakdown_figure(data, linear)
        summary.to_csv(DATA / f"14_full_camera_pass_breakdown_{suffix}.csv", index=False)
        save_figure(
            figure,
            f"14_full_camera_pass_breakdown_{suffix}",
            f"Execution-order pass breakdown ({suffix})",
            [
                "results/14_real_full_camera_linear_control/"
                "14_real_full_camera_linear_control.csv"
            ],
            "success rows; clear operations excluded; pass colors follow execution order",
        )

    speed = paired_speedup(data, ["scene", "linear_gbuffer"])
    fig, axis = plt.subplots(figsize=(9, 5))
    x = np.arange(len(SCENE_ORDER))
    width = 0.34
    for index, linear in enumerate((False, True)):
        part = speed[speed["linear_gbuffer"] == linear].set_index("scene").reindex(SCENE_ORDER)
        axis.bar(
            x + (index - 0.5) * width,
            part["deferred_over_visbuf"],
            width,
            label="Linear control" if linear else "sRGB ABI",
            color="#377eb8" if linear else "#e41a1c",
        )
    axis.axhline(1.0, color="black", linewidth=1)
    axis.set_xticks(x, SCENE_ORDER)
    axis.set_ylabel("DeferredPrepass / VisBuf")
    axis.set_title("Full-camera renderer comparison by scene")
    axis.legend()
    speed.to_csv(DATA / "15_full_camera_scene_speedup.csv", index=False)
    save_figure(
        fig,
        "15_full_camera_scene_comparison",
        "Full-camera renderer speed ratio by scene and G-buffer ABI",
        [
            "results/14_real_full_camera_linear_control/"
            "14_real_full_camera_linear_control.csv"
        ],
        "success rows; full camera",
    )
    return data


def load_profile_sidecar(config: str, run_index: int) -> pd.DataFrame:
    path = (
        RESULTS
        / config
        / f"{config}_runs"
        / f"run_{run_index:05d}.csv_{run_index}_result.csv"
    )
    data = pd.read_csv(path)
    numeric(data, data.columns)
    return data


def plot_full_camera_timelines(full_data: pd.DataFrame) -> None:
    config = "14_real_full_camera_linear_control"
    for scene in SCENE_ORDER:
        rows = full_data[
            (full_data["scene"] == scene) & (~full_data["linear_gbuffer"])
        ]
        fig, axes = plt.subplots(2, 1, figsize=(14, 9), sharex=True)
        profile_rows = []
        for _, row in rows.iterrows():
            run_index = int(row["runner_run_index"])
            profile = load_profile_sidecar(config, run_index)
            profile["renderer"] = row["renderer"]
            profile["scene"] = scene
            profile_rows.append(profile)
            axes[0].plot(
                profile["frame"],
                profile["total"],
                color=RENDERER_COLORS[row["renderer"]],
                linewidth=1.8,
                label=row["renderer"],
            )
            ordered = [name for name in PASS_ORDER if name in profile.columns]
            for pass_name in ordered:
                axes[1].plot(
                    profile["frame"],
                    profile[pass_name],
                    color=PASS_COLORS[pass_name],
                    linewidth=1.2,
                    linestyle="-" if row["renderer"] == "DeferredPrepass" else "--",
                    label=f"{row['renderer']} · {PASS_LABELS[pass_name]}",
                )
        axes[0].set_ylabel("Total GPU time (ms)")
        axes[0].set_title("Total timeline")
        axes[0].legend()
        axes[1].set_xlabel("Measurement frame")
        axes[1].set_ylabel("Pass GPU time (ms)")
        axes[1].set_title("Functional passes in execution-order colors")
        axes[1].legend(fontsize=7, ncol=3)
        fig.suptitle(f"{scene} full-camera timeline · sRGB ABI")
        combined = pd.concat(profile_rows, ignore_index=True)
        safe_scene = scene.lower().replace(" ", "_")
        combined.to_csv(DATA / f"16_timeline_{safe_scene}.csv", index=False)
        save_figure(
            fig,
            f"16_timeline_{safe_scene}",
            f"{scene} full-camera total and pass timelines",
            [
                "results/14_real_full_camera_linear_control/"
                "14_real_full_camera_linear_control_runs/*_result.csv"
            ],
            "success rows; sRGB ABI; 60-frame profile windows",
        )


def plot_ablation() -> None:
    data = load_config("15_real_texture_vfc_ablation")
    data["texture"] = data["param_to_load_texture"].astype(int).map({0: "Texture off", 1: "Texture on"})
    data["vfc"] = data["param_use_vfc"].astype(int).map({0: "VFC off", 1: "VFC on"})
    data["condition"] = data["texture"] + "\n" + data["vfc"]
    conditions = ["Texture off\nVFC off", "Texture off\nVFC on", "Texture on\nVFC off", "Texture on\nVFC on"]
    fig, axes = plt.subplots(1, 3, figsize=(17, 5.5), sharey=False)
    summary_rows = []
    for axis, scene in zip(axes, SCENE_ORDER):
        scene_data = data[data["scene"] == scene]
        x = np.arange(len(conditions))
        width = 0.36
        for index, renderer in enumerate(("DeferredPrepass", "VisBuf")):
            part = scene_data[scene_data["renderer"] == renderer].set_index("condition").reindex(conditions)
            values = part["total_time_avg_ms"].to_numpy()
            axis.bar(
                x + (index - 0.5) * width,
                values,
                width,
                color=RENDERER_COLORS[renderer],
                label=renderer,
            )
            for condition, value in zip(conditions, values):
                summary_rows.append(
                    {
                        "scene": scene,
                        "condition": condition.replace("\n", " / "),
                        "renderer": renderer,
                        "total_time_avg_ms": value,
                    }
                )
        axis.set_xticks(x, conditions, rotation=25, ha="right")
        axis.set_title(scene)
        axis.set_ylabel("Total GPU time (ms)")
        axis.legend(fontsize=8)
    fig.suptitle("Texture loading × VFC ablation")
    pd.DataFrame(summary_rows).to_csv(DATA / "17_texture_vfc_ablation.csv", index=False)
    save_figure(
        fig,
        "17_real_texture_vfc_ablation",
        "Texture-loading and view-frustum-culling ablation",
        [
            "results/15_real_texture_vfc_ablation/"
            "15_real_texture_vfc_ablation.csv"
        ],
        "success rows; 600-frame camera prefix",
    )


def align_raster_stats(stats: pd.DataFrame, window_frames: int = 60) -> pd.DataFrame:
    """Average raw raster stats into GPU profile windows (early ex10 method)."""
    result = stats.copy()
    result["measurement_frame"] = result["frame"] - result["frame"].min()
    result["frame"] = (
        result["measurement_frame"] // window_frames
    ) * window_frames
    numeric(result, result.columns)
    columns = [
        column
        for column in result.columns
        if column not in {"frame", "measurement_frame"}
    ]
    return result.groupby("frame", as_index=False)[columns].mean()


def load_raster_stats(run_index: int) -> pd.DataFrame:
    path = (
        RESULTS
        / "16_real_software_raster_reference"
        / "16_real_software_raster_reference_runs"
        / f"run_{run_index:05d}_{run_index}_raster_stats.csv"
    )
    return pd.read_csv(path)


def plot_raster_stats(full_data: pd.DataFrame) -> None:
    raster_main = load_config("16_real_software_raster_reference")
    raster_frames = []
    summary_rows = []
    metrics = [
        "triangle_count",
        "total_fragments",
        "covered_pixels",
        "avg_overdraw",
        "quad_efficiency",
        "quad_waste_lanes",
    ]
    for _, row in raster_main.iterrows():
        run_index = int(row["runner_run_index"])
        stats = load_raster_stats(run_index)
        stats["scene"] = row["scene"]
        stats["measurement_frame"] = stats["frame"] - stats["frame"].min()
        raster_frames.append(stats)
        for metric in metrics:
            summary_rows.append(
                {
                    "scene": row["scene"],
                    "metric": metric,
                    "mean": stats[metric].mean(),
                    "median": stats[metric].median(),
                    "p90": stats[metric].quantile(0.90),
                    "p99": stats[metric].quantile(0.99),
                }
            )
    raw = pd.concat(raster_frames, ignore_index=True)
    summary = pd.DataFrame(summary_rows)
    raw.to_csv(DATA / "18_software_raster_frames.csv", index=False)
    summary.to_csv(DATA / "18_software_raster_summary.csv", index=False)

    fig, axes = plt.subplots(2, 3, figsize=(16, 9))
    for axis, metric in zip(axes.ravel(), metrics):
        part = summary[summary["metric"] == metric].set_index("scene").reindex(SCENE_ORDER)
        axis.bar(SCENE_ORDER, part["mean"], color="#7570b3")
        axis.set_title(metric.replace("_", " "))
        axis.tick_params(axis="x", rotation=20)
    fig.suptitle("Full-camera software-raster workload proxies")
    save_figure(
        fig,
        "18_software_raster_scene_summary",
        "Scene means for triangle, fragment, coverage, overdraw, and quad metrics",
        [
            "results/16_real_software_raster_reference/"
            "16_real_software_raster_reference_runs/*_raster_stats.csv"
        ],
        "all measured software-raster frames; blank-view zero-coverage frames retained",
    )

    correlation_rows = []
    scatter_fig, scatter_axes = plt.subplots(3, 2, figsize=(14, 14))
    for row_index, scene in enumerate(SCENE_ORDER):
        raster_row = raster_main[raster_main["scene"] == scene].iloc[0]
        raster = align_raster_stats(
            load_raster_stats(int(raster_row["runner_run_index"]))
        )
        perf_rows = full_data[
            (full_data["scene"] == scene) & (~full_data["linear_gbuffer"])
        ]
        for column_index, renderer in enumerate(("DeferredPrepass", "VisBuf")):
            perf_row = perf_rows[perf_rows["renderer"] == renderer].iloc[0]
            profile = load_profile_sidecar(
                "14_real_full_camera_linear_control",
                int(perf_row["runner_run_index"]),
            )
            merged = profile.merge(raster, on="frame", how="inner")
            axis = scatter_axes[row_index, column_index]
            axis.scatter(
                merged["avg_overdraw"],
                merged["total"],
                c=merged["quad_efficiency"],
                cmap="viridis",
                s=24,
                alpha=0.8,
            )
            axis.set_xlabel("Average overdraw")
            axis.set_ylabel("Total GPU time (ms)")
            axis.set_title(f"{scene} · {renderer}")
            gpu_columns = [
                column
                for column in ["total", *PASS_ORDER, "index_count"]
                if column in merged
            ]
            raster_columns = [
                "triangle_count",
                "total_fragments",
                "covered_pixels",
                "avg_overdraw",
                "quad_efficiency",
                "quad_waste_lanes",
            ]
            for raster_metric in raster_columns:
                for gpu_metric in gpu_columns:
                    correlation_rows.append(
                        {
                            "scene": scene,
                            "renderer": renderer,
                            "raster_metric": raster_metric,
                            "gpu_metric": gpu_metric,
                            "pearson_r": merged[raster_metric].corr(merged[gpu_metric]),
                            "window_count": len(merged),
                        }
                    )
    correlations = pd.DataFrame(correlation_rows)
    correlations.to_csv(DATA / "19_raster_gpu_correlations.csv", index=False)
    save_figure(
        scatter_fig,
        "19_raster_overdraw_vs_gpu_time",
        "Aligned profile-window overdraw versus renderer total; color is quad efficiency",
        [
            "results/14_real_full_camera_linear_control/*_result.csv",
            "results/16_real_software_raster_reference/*_raster_stats.csv",
        ],
        "sRGB full-camera runs; software stats aligned to 60-frame windows",
    )

    overview = correlations[correlations["gpu_metric"] == "total"].copy()
    overview["column"] = overview["scene"] + "\n" + overview["renderer"]
    pivot = overview.pivot(
        index="raster_metric",
        columns="column",
        values="pearson_r",
    )
    desired_columns = [
        f"{scene}\n{renderer}"
        for scene in SCENE_ORDER
        for renderer in ("DeferredPrepass", "VisBuf")
    ]
    pivot = pivot.reindex(
        index=[
            "triangle_count",
            "total_fragments",
            "covered_pixels",
            "avg_overdraw",
            "quad_efficiency",
            "quad_waste_lanes",
        ],
        columns=desired_columns,
    )
    fig, axis = plt.subplots(figsize=(12, 6))
    image = axis.imshow(pivot.to_numpy(), cmap="coolwarm", vmin=-1, vmax=1, aspect="auto")
    axis.set_xticks(range(len(pivot.columns)), pivot.columns, rotation=35, ha="right")
    axis.set_yticks(range(len(pivot.index)), [value.replace("_", " ") for value in pivot.index])
    for y in range(len(pivot.index)):
        for x in range(len(pivot.columns)):
            value = pivot.iloc[y, x]
            axis.text(x, y, f"{value:+.2f}", ha="center", va="center", fontsize=8)
    colorbar = fig.colorbar(image, ax=axis)
    colorbar.set_label("Pearson correlation with total GPU time")
    axis.set_title("Software-raster metric correlation overview")
    save_figure(
        fig,
        "20_raster_gpu_correlation_overview",
        "Pearson correlation between aligned software-raster metrics and total time",
        ["data/19_raster_gpu_correlations.csv"],
        "sRGB full-camera runs; 60-frame profile windows",
    )


def plot_campaign_status(manifest: dict[str, object]) -> None:
    configs = pd.DataFrame(manifest["configs"])
    configs[
        [
            "config",
            "expected_runs",
            "successful_runs",
            "salvaged_runs",
            "failed_runs",
            "skipped_runs",
            "status",
        ]
    ].to_csv(DATA / "00_campaign_status.csv", index=False)
    failures = configs[
        (configs["failed_runs"] > 0)
        | (configs["skipped_runs"] > 0)
        | (configs["salvaged_runs"] > 0)
    ][
        [
            "config",
            "salvaged_runs",
            "failed_runs",
            "skipped_runs",
            "error_summary",
        ]
    ]
    failures.to_csv(DATA / "failed_skipped_cases.csv", index=False)

    fig, axis = plt.subplots(figsize=(13, 6))
    y = np.arange(len(configs))
    axis.barh(y, configs["successful_runs"], color="#4daf4a", label="Success")
    axis.barh(
        y,
        configs["failed_runs"] + configs["salvaged_runs"] + configs["skipped_runs"],
        left=configs["successful_runs"],
        color="#e41a1c",
        label="Other",
    )
    axis.set_yticks(y, configs["config"].str.replace(".json", "", regex=False), fontsize=8)
    axis.invert_yaxis()
    axis.set_xlabel("Run count")
    axis.set_title(
        f"Campaign completion: {manifest['successful_runs']}/{manifest['expected_runs']} success"
    )
    axis.legend()
    save_figure(
        fig,
        "00_campaign_completion",
        "Expected and successful runs per JSON",
        ["results/_campaign_manifest.json"],
        "all manifest statuses; failures/skips are not plotted as measurements",
    )


def write_supporting_tables(
    all_rows: pd.DataFrame,
    pass_long: pd.DataFrame,
) -> None:
    all_rows.to_csv(DATA / "all_success_rows.csv", index=False)
    pass_long.to_csv(DATA / "all_pass_times_long.csv", index=False)
    hardware_rows = [
        {
            "scope": "followup_experiments/results",
            "hardware": HARDWARE,
            "use": "current plots and conclusions",
            "comparable_pool": "RTX5060Ti-20260730",
        },
        {
            "scope": "datas/experiments archive",
            "hardware": "NVIDIA GeForce RTX 5070",
            "use": "historical context and plotting-method reference only",
            "comparable_pool": "RTX5070-archive",
        },
    ]
    pd.DataFrame(hardware_rows).to_csv(DATA / "hardware_sources.csv", index=False)


def write_index_and_readme() -> None:
    index = pd.DataFrame(PLOT_INDEX)
    index.to_csv(PLOTS / "plot_index.csv", index=False)
    lines = [
        "# Follow-up experiment plots",
        "",
        f"- Current measurement hardware: **{HARDWARE}**",
        "- Earlier archived `datas/` results: **NVIDIA GeForce RTX 5070**",
        "- The two hardware pools are labeled separately and are not numerically pooled.",
        "- Only `runner_status=success` rows are used as measurements.",
        "- Clear operations and PSO-count metrics are not included.",
        "- Pass stack order and colors follow renderer execution order; depth pre-pass",
        "  and visibility use related blue colors.",
        "",
        "## Reproduction",
        "",
        "```powershell",
        ".\\.venv\\Scripts\\python.exe -m pip install -r scripts\\followup_experiments\\plot_requirements.txt",
        ".\\.venv\\Scripts\\python.exe scripts\\followup_experiments\\plot_results.py",
        "```",
        "",
        "The script cleans only `scripts/followup_experiments/plots`, then rebuilds",
        "PNG, SVG, plot-ready CSV tables, and this index.",
        "",
        "## Reused early plotting ideas",
        "",
        "- `ex10/.../03_pass_by_frame_plots.py`: pass timelines",
        "- `ex10/.../04_median_pass_breakdown.py`: stacked pass breakdown",
        "- `ex10/.../11_bistro_all_raster_stat_figures.py`: raster/profile alignment",
        "- `ex12-18 scripts/plot_sponza_raster_metric_comparison.py`: correlation overview",
        "- `ex5` and `ex6` orthogonal plot bundles: direct sampled heatmaps and",
        "  equal-performance contours",
        "",
        "## Plot index",
        "",
        "| Plot | Description |",
        "|---|---|",
    ]
    for row in PLOT_INDEX:
        lines.append(f"| [{row['plot']}](png/{row['plot']}.png) | {row['description']} |")
    lines.extend(
        [
            "",
            "## Failed and skipped cases",
            "",
            "`data/failed_skipped_cases.csv` has headers but no rows because all",
            "1,301 expected runs completed successfully.",
            "",
        ]
    )
    (PLOTS / "README.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    clean_output()
    configure_style()
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    if manifest.get("campaign_status") != "completed":
        raise RuntimeError("Campaign must be finalized before plotting.")
    if int(manifest.get("successful_runs", 0)) != int(manifest.get("expected_runs", -1)):
        raise RuntimeError("Plotting requires a fully successful campaign.")

    all_rows = load_all_success()
    pass_long = build_pass_long(all_rows)
    write_supporting_tables(all_rows, pass_long)
    plot_campaign_status(manifest)
    plot_material_and_class_responses()
    plot_locality_and_diversity()
    plot_phase_map()
    plot_material_class_matrix()
    plot_scaling_and_seed()
    plot_linear_control()
    plot_real_sweeps()
    full_data = plot_full_camera_summary()
    plot_full_camera_timelines(full_data)
    plot_ablation()
    plot_raster_stats(full_data)
    write_index_and_readme()
    print(f"Created {len(PLOT_INDEX)} plots in {PLOTS}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
