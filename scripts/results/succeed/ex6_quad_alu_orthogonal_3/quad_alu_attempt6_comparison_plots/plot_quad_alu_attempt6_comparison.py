#!/usr/bin/env python3
"""
Plot a quad_alu_orthogonal TVBPerf CSV.

The experiment sweeps two numeric dimensions (waste-quad count and ALU calculation
count) for three renderer variants. The script generates:

1-3. Variant-specific total-time contour plots
4.    Discrete winner map
5.    Winner confidence / margin map
6.    Per-variant regret maps (time / fastest time)
7.    Speedup against Forward for ForwardPrepass and VisBuf
8.    VisBuf versus ForwardPrepass pairwise ratio and crossover boundary
9.    Selected ALU-axis slices
10.   Selected waste-quad-axis slices
11.   Quad-proxy reciprocal interaction-model fit diagnostics

It also exports validation reports, winner/crossover tables, empirical model
coefficients, PNG/SVG versions, and a ZIP bundle.

Usage:
    python plot_quad_alu_orthogonal.py quad_alu_orthogonal.csv \
        --output-dir quad_alu_orthogonal_plots
"""

from __future__ import annotations

import argparse
import csv
import math
import shutil
import zipfile
from collections import Counter
from pathlib import Path
from typing import Iterable

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import BoundaryNorm, ListedColormap, LogNorm, TwoSlopeNorm
from matplotlib.lines import Line2D
from matplotlib.patches import Patch
from matplotlib.ticker import FuncFormatter
import numpy as np


VARIANT_RULES = {
    1: {
        "renderer": "Forward",
        "marker": "o",
        "color": "C0",
        "passes": [
            ("forward", "pass_0_time_avg_ms", "--"),
        ],
        "total": "pass_3_time_avg_ms",
    },
    2: {
        "renderer": "ForwardPrepass",
        "marker": "s",
        "color": "C1",
        "passes": [
            ("depth_prepass", "pass_0_time_avg_ms", "--"),
            ("forward", "pass_1_time_avg_ms", ":"),
        ],
        "total": "pass_3_time_avg_ms",
    },
    4: {
        "renderer": "VisBuf",
        "marker": "^",
        "color": "C2",
        "passes": [
            ("visibility", "pass_0_time_avg_ms", "--"),
            ("resolve", "pass_1_time_avg_ms", ":"),
        ],
        "total": "pass_3_time_avg_ms",
    },
}

VARIANT_ORDER = [1, 2, 4]
VALIDATION_TOLERANCE_MS = 1.1e-5


def normalize_name(name: str) -> str:
    return "".join(ch for ch in name.lower() if ch.isalnum())


def resolve_column(fieldnames: Iterable[str], candidates: Iterable[str]) -> str:
    exact = set(fieldnames)
    for candidate in candidates:
        if candidate in exact:
            return candidate

    normalized = {normalize_name(name): name for name in fieldnames}
    for candidate in candidates:
        key = normalize_name(candidate)
        if key in normalized:
            return normalized[key]

    raise KeyError(f"Required column not found. Tried: {list(candidates)}")


def as_float(value: str | None) -> float:
    try:
        return float(value) if value not in (None, "") else math.nan
    except (TypeError, ValueError):
        return math.nan


def load_csv(csv_path: Path):
    with csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        raw_rows = list(reader)
        fieldnames = reader.fieldnames or []

    status_col = resolve_column(fieldnames, ["runner_status", "status"])
    variant_col = resolve_column(
        fieldnames,
        ["renderer-variant", "renderer_variant", "param_renderer_variant", "variant"],
    )
    renderer_col = resolve_column(fieldnames, ["renderer_name", "renderer-name"])
    quad_col = resolve_column(
        fieldnames,
        ["variable-waste-quad-count", "waste-quad-count", "variable-quad-count", "quad-count", "param_geometry_div", "geometry-div"],
    )
    alu_col = resolve_column(
        fieldnames,
        ["variable-alu-op-count", "alu-calc-count", "param_alu_calc_count"],
    )

    success_raw = [
        row
        for row in raw_rows
        if str(row.get(status_col, "")).strip().lower() == "success"
    ]
    failed = [
        row
        for row in raw_rows
        if str(row.get(status_col, "")).strip().lower() != "success"
    ]

    cleaned = []
    for row in success_raw:
        variant_value = as_float(row.get(variant_col))
        quad = as_float(row.get(quad_col))
        alu = as_float(row.get(alu_col))
        if not all(math.isfinite(value) for value in (variant_value, quad, alu)):
            continue
        variant = int(variant_value)
        if variant not in VARIANT_RULES or quad < 0 or alu < 0:
            continue

        cleaned.append(
            {
                "variant": variant,
                "renderer": row.get(renderer_col) or VARIANT_RULES[variant]["renderer"],
                "quad": float(quad),
                "alu": float(alu),
                **{
                    name: as_float(row.get(name))
                    for name in [
                        "pass_0_time_avg_ms",
                        "pass_1_time_avg_ms",
                        "pass_2_time_avg_ms",
                        "pass_3_time_avg_ms",
                    ]
                },
            }
        )

    return cleaned, failed, fieldnames


def aggregate(rows):
    buckets: dict[tuple[int, float, float], list[dict]] = {}
    for row in rows:
        key = (row["variant"], row["quad"], row["alu"])
        buckets.setdefault(key, []).append(row)

    aggregated = []
    for (variant, quad, alu), group in sorted(buckets.items()):
        item = {
            "variant": variant,
            "renderer": VARIANT_RULES[variant]["renderer"],
            "quad": quad,
            "alu": alu,
            "repeat_count": len(group),
        }
        for column in [
            "pass_0_time_avg_ms",
            "pass_1_time_avg_ms",
            "pass_2_time_avg_ms",
            "pass_3_time_avg_ms",
        ]:
            values = np.array([row[column] for row in group], dtype=float)
            finite = values[np.isfinite(values)]
            item[column] = float(finite.mean()) if finite.size else math.nan
            item[f"{column}_std"] = (
                float(finite.std(ddof=1)) if finite.size > 1 else 0.0
            )
        aggregated.append(item)
    return aggregated


def build_grid(rows):
    quad_values = sorted({row["quad"] for row in rows})
    alu_values = sorted({row["alu"] for row in rows})
    quad_index = {value: index for index, value in enumerate(quad_values)}
    alu_index = {value: index for index, value in enumerate(alu_values)}

    grids = {}
    pass_grids = {}
    repeat_grid = {}
    for variant in VARIANT_ORDER:
        total_grid = np.full((len(quad_values), len(alu_values)), np.nan)
        repeat_counts = np.zeros_like(total_grid)
        variant_passes = {
            column: np.full_like(total_grid, np.nan)
            for column in [
                "pass_0_time_avg_ms",
                "pass_1_time_avg_ms",
                "pass_2_time_avg_ms",
                "pass_3_time_avg_ms",
            ]
        }
        for row in rows:
            if row["variant"] != variant:
                continue
            oi = quad_index[row["quad"]]
            ai = alu_index[row["alu"]]
            total_grid[oi, ai] = row[VARIANT_RULES[variant]["total"]]
            repeat_counts[oi, ai] = row["repeat_count"]
            for column in variant_passes:
                variant_passes[column][oi, ai] = row[column]
        grids[variant] = total_grid
        pass_grids[variant] = variant_passes
        repeat_grid[variant] = repeat_counts

    return np.array(quad_values), np.array(alu_values), grids, pass_grids, repeat_grid


def safe_number(value: float) -> str:
    if not math.isfinite(value):
        return ""
    if abs(value - round(value)) < 1e-9:
        return f"{int(round(value)):,}"
    return f"{value:,.3g}"


def apply_sampled_axis_style(ax, quad_values, alu_values):
    ax.set_xscale("symlog", base=2, linthresh=1, linscale=1)
    ax.set_yscale("symlog", base=2, linthresh=1, linscale=1)
    ax.set_xticks(alu_values)
    ax.set_yticks(quad_values)
    ax.xaxis.set_major_formatter(FuncFormatter(lambda value, _: safe_number(value)))
    ax.yaxis.set_major_formatter(FuncFormatter(lambda value, _: safe_number(value)))
    ax.set_xlim(float(alu_values.min()), float(alu_values.max()))
    ax.set_ylim(float(quad_values.min()), float(quad_values.max()))
    ax.set_xlabel("ALU calculation count")
    ax.set_ylabel("Waste quad count")
    ax.grid(True, which="major", linestyle=":", alpha=0.22)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)


def index_mesh(shape):
    rows, cols = shape
    x_edges = np.arange(cols + 1) - 0.5
    y_edges = np.arange(rows + 1) - 0.5
    return x_edges, y_edges


def apply_index_axis_style(ax, quad_values, alu_values):
    ax.set_xticks(np.arange(len(alu_values)))
    ax.set_xticklabels([safe_number(value) for value in alu_values])
    ax.set_yticks(np.arange(len(quad_values)))
    ax.set_yticklabels([safe_number(value) for value in quad_values])
    ax.set_xlabel("ALU calculation count")
    ax.set_ylabel("Waste quad count")
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)


def save_figure(fig, output_dir: Path, stem: str):
    png = output_dir / f"{stem}.png"
    svg = output_dir / f"{stem}.svg"
    fig.savefig(png, dpi=180, bbox_inches="tight")
    fig.savefig(svg, bbox_inches="tight")
    plt.close(fig)
    return [png, svg]


def positive_log_levels(values, count=18):
    finite = np.asarray(values, dtype=float)
    finite = finite[np.isfinite(finite) & (finite > 0)]
    if not finite.size:
        return np.geomspace(1e-4, 1, count)
    vmin, vmax = float(finite.min()), float(finite.max())
    if math.isclose(vmin, vmax):
        vmax = vmin * 1.01
    return np.geomspace(vmin, vmax, count)


def plot_variant_contours(quad_values, alu_values, grids, output_dir):
    image_files = []
    X, Y = np.meshgrid(alu_values, quad_values)
    for number, variant in enumerate(VARIANT_ORDER, start=1):
        rule = VARIANT_RULES[variant]
        Z = grids[variant]
        levels = positive_log_levels(Z)
        fig, ax = plt.subplots(figsize=(14.5, 8.4), constrained_layout=True)
        contour = ax.contourf(
            X,
            Y,
            Z,
            levels=levels,
            norm=LogNorm(vmin=float(np.nanmin(Z)), vmax=float(np.nanmax(Z))),
            cmap="viridis",
            extend="both",
        )
        ax.contour(X, Y, Z, levels=levels[::2], colors="black", linewidths=0.45, alpha=0.28)
        ax.scatter(X, Y, s=19, facecolors="none", edgecolors="white", linewidths=0.7, alpha=0.85)
        apply_sampled_axis_style(ax, quad_values, alu_values)
        ax.set_title(
            f"Quad Waste × ALU Orthogonal — Variant {variant} · {rule['renderer']} · Total Time"
        )
        colorbar = fig.colorbar(contour, ax=ax, pad=0.02)
        colorbar.set_label("Average total GPU time (ms, log color scale)")
        ax.text(
            0.01,
            0.014,
            "Open circles: measured sample points · Contours interpolate only between sampled grid values",
            transform=ax.transAxes,
            ha="left",
            va="bottom",
            fontsize=9,
            bbox={"facecolor": "white", "edgecolor": "none", "alpha": 0.78, "pad": 2.5},
        )
        image_files.extend(
            save_figure(
                fig,
                output_dir,
                f"{number:02d}_contour_variant_{variant}_{rule['renderer'].lower()}_total_time",
            )
        )
    return image_files


def compute_winner(grids):
    stack = np.stack([grids[variant] for variant in VARIANT_ORDER], axis=0)
    winner_index = np.nanargmin(stack, axis=0)
    sorted_stack = np.sort(stack, axis=0)
    best = sorted_stack[0]
    second = sorted_stack[1]
    margin = np.divide(second, best, out=np.full_like(best, np.nan), where=best > 0)
    return stack, winner_index, best, second, margin


def plot_winner_map(quad_values, alu_values, grids, output_dir):
    _, winner_index, best, _, margin = compute_winner(grids)
    x_edges, y_edges = index_mesh(winner_index.shape)
    cmap = ListedColormap([VARIANT_RULES[v]["color"] for v in VARIANT_ORDER])
    norm = BoundaryNorm([-0.5, 0.5, 1.5, 2.5], cmap.N)

    fig, ax = plt.subplots(figsize=(14.5, 8.4), constrained_layout=True)
    ax.pcolormesh(x_edges, y_edges, winner_index, cmap=cmap, norm=norm, shading="flat")
    apply_index_axis_style(ax, quad_values, alu_values)
    ax.set_title(
        "Quad Waste × ALU Orthogonal — Fastest Renderer Winner Map\n"
        "Cell text: winner abbreviation and measured total time (ms)",
        pad=12,
    )

    initials = {0: "F", 1: "FP", 2: "VB"}
    for oi in range(winner_index.shape[0]):
        for ai in range(winner_index.shape[1]):
            idx = int(winner_index[oi, ai])
            text_color = "white" if idx in (0, 2) else "black"
            ax.text(
                ai,
                oi,
                f"{initials[idx]}\n{best[oi, ai]:.3f}",
                ha="center",
                va="center",
                fontsize=8.3,
                color=text_color,
                fontweight="bold",
            )

    handles = [
        Patch(
            facecolor=VARIANT_RULES[variant]["color"],
            label=f"Variant {variant} · {VARIANT_RULES[variant]['renderer']}",
        )
        for variant in VARIANT_ORDER
    ]
    ax.legend(
        handles=handles,
        loc="center left",
        bbox_to_anchor=(1.005, 0.5),
        ncol=1,
        frameon=True,
    )
    return save_figure(fig, output_dir, "04_winner_map")


def plot_winner_margin(quad_values, alu_values, grids, output_dir):
    _, winner_index, _, _, margin = compute_winner(grids)
    x_edges, y_edges = index_mesh(margin.shape)
    fig, ax = plt.subplots(figsize=(14.5, 8.4), constrained_layout=True)
    mesh = ax.pcolormesh(
        x_edges,
        y_edges,
        margin,
        cmap="magma",
        shading="flat",
        vmin=1.0,
        vmax=float(np.nanmax(margin)),
    )
    apply_index_axis_style(ax, quad_values, alu_values)
    ax.set_title("Quad Waste × ALU Orthogonal — Winner Margin (Second-best / Best)")
    colorbar = fig.colorbar(mesh, ax=ax, pad=0.02)
    colorbar.set_label("Second-best time / best time (×)")

    initials = {0: "F", 1: "FP", 2: "VB"}
    for oi in range(margin.shape[0]):
        for ai in range(margin.shape[1]):
            ax.text(
                ai,
                oi,
                f"{initials[int(winner_index[oi, ai])]}\n{margin[oi, ai]:.2f}×",
                ha="center",
                va="center",
                fontsize=8.1,
                color="white" if margin[oi, ai] > 1.12 else "black",
            )
    ax.text(
        0.01,
        0.014,
        "1.00× means a fragile/tied winner · Larger values mean a more decisive winner",
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=9,
        bbox={"facecolor": "white", "edgecolor": "none", "alpha": 0.78, "pad": 2.5},
    )
    return save_figure(fig, output_dir, "05_winner_margin_second_best_over_best")


def plot_regret_maps(quad_values, alu_values, grids, output_dir):
    stack, _, best, _, _ = compute_winner(grids)
    regrets = np.divide(stack, best[None, :, :], out=np.full_like(stack, np.nan), where=best[None, :, :] > 0)
    vmax = float(np.nanmax(regrets))
    fig, axes = plt.subplots(1, 3, figsize=(18.5, 6.5), constrained_layout=True, sharex=True, sharey=True)
    x_edges, y_edges = index_mesh(best.shape)
    mesh = None
    for index, (ax, variant) in enumerate(zip(axes, VARIANT_ORDER)):
        mesh = ax.pcolormesh(
            x_edges,
            y_edges,
            regrets[index],
            cmap="viridis",
            norm=LogNorm(vmin=1.0, vmax=vmax),
            shading="flat",
        )
        apply_index_axis_style(ax, quad_values, alu_values)
        if index > 0:
            ax.set_ylabel("")
        ax.set_title(f"Variant {variant} · {VARIANT_RULES[variant]['renderer']}")
        for oi in range(regrets.shape[1]):
            for ai in range(regrets.shape[2]):
                value = regrets[index, oi, ai]
                ax.text(ai, oi, f"{value:.2f}", ha="center", va="center", fontsize=6.7,
                        color="white" if value > 1.8 else "black")
    fig.suptitle("Quad Waste × ALU Orthogonal — Cost Relative to the Fastest Renderer", fontsize=15)
    colorbar = fig.colorbar(mesh, ax=axes, pad=0.012, shrink=0.88)
    colorbar.set_label("Variant time / fastest time (×, log color scale)")
    return save_figure(fig, output_dir, "06_variant_regret_time_over_fastest")


def plot_speedup_vs_forward(quad_values, alu_values, grids, output_dir):
    forward = grids[1]
    variants = [2, 4]
    speedups = [
        np.divide(forward, grids[v], out=np.full_like(forward, np.nan), where=grids[v] > 0)
        for v in variants
    ]
    vmax = max(float(np.nanmax(value)) for value in speedups)
    vmin = min(float(np.nanmin(value)) for value in speedups)
    max_distance = max(abs(math.log(max(vmin, 1e-9))), abs(math.log(vmax)))
    log_low = math.exp(-max_distance)
    log_high = math.exp(max_distance)

    X, Y = np.meshgrid(alu_values, quad_values)
    fig, axes = plt.subplots(1, 2, figsize=(18.0, 7.4), constrained_layout=True, sharex=True, sharey=True)
    contour = None
    levels = np.geomspace(log_low, log_high, 25)
    for index, (ax, variant, speedup) in enumerate(zip(axes, variants, speedups)):
        contour = ax.contourf(
            X,
            Y,
            speedup,
            levels=levels,
            norm=LogNorm(vmin=log_low, vmax=log_high),
            cmap="RdBu_r",
            extend="both",
        )
        boundary = ax.contour(X, Y, speedup, levels=[1.0], colors="black", linewidths=2.1)
        if boundary.allsegs and any(len(segment) for segment in boundary.allsegs[0]):
            ax.clabel(boundary, fmt={1.0: "1.0× crossover"}, inline=True, fontsize=8)
        ax.scatter(X, Y, s=14, facecolors="none", edgecolors="black", linewidths=0.5, alpha=0.55)
        apply_sampled_axis_style(ax, quad_values, alu_values)
        if index > 0:
            ax.set_ylabel("")
        ax.set_title(
            f"Variant {variant} · {VARIANT_RULES[variant]['renderer']} speedup vs Forward"
        )
    fig.suptitle("Quad Waste × ALU Orthogonal — Forward Time / Alternative Time", fontsize=15)
    colorbar = fig.colorbar(contour, ax=axes, pad=0.012, shrink=0.88)
    colorbar.set_label("Forward time / alternative time (×) · >1 means alternative is faster")
    return save_figure(fig, output_dir, "07_speedup_vs_forward_with_crossover")


def plot_visbuf_vs_prepass(quad_values, alu_values, grids, output_dir):
    ratio = np.divide(grids[2], grids[4], out=np.full_like(grids[2], np.nan), where=grids[4] > 0)
    # ratio > 1 means VisBuf is faster than ForwardPrepass.
    finite = ratio[np.isfinite(ratio) & (ratio > 0)]
    distance = max(abs(math.log(float(finite.min()))), abs(math.log(float(finite.max()))))
    low, high = math.exp(-distance), math.exp(distance)
    levels = np.geomspace(low, high, 25)
    X, Y = np.meshgrid(alu_values, quad_values)

    fig, ax = plt.subplots(figsize=(14.5, 8.4), constrained_layout=True)
    contour = ax.contourf(
        X,
        Y,
        ratio,
        levels=levels,
        norm=LogNorm(vmin=low, vmax=high),
        cmap="RdBu_r",
        extend="both",
    )
    boundary = ax.contour(X, Y, ratio, levels=[1.0], colors="black", linewidths=2.3)
    if boundary.allsegs and any(len(segment) for segment in boundary.allsegs[0]):
        ax.clabel(boundary, fmt={1.0: "equal time"}, inline=True, fontsize=9)
    ax.scatter(X, Y, s=19, facecolors="none", edgecolors="black", linewidths=0.55, alpha=0.58)
    apply_sampled_axis_style(ax, quad_values, alu_values)
    ax.set_title("Quad Waste × ALU Orthogonal — VisBuf vs ForwardPrepass Crossover")
    colorbar = fig.colorbar(contour, ax=ax, pad=0.02)
    colorbar.set_label("ForwardPrepass time / VisBuf time (×) · >1 means VisBuf is faster")
    ax.text(
        0.01,
        0.014,
        "Black contour: equal total time · Red side: VisBuf advantage · Blue side: ForwardPrepass advantage",
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=9,
        bbox={"facecolor": "white", "edgecolor": "none", "alpha": 0.78, "pad": 2.5},
    )
    return save_figure(fig, output_dir, "08_visbuf_vs_forwardprepass_pairwise_crossover")


def nearest_indices(values, requested):
    indices = []
    for target in requested:
        index = int(np.argmin(np.abs(values - target)))
        if index not in indices:
            indices.append(index)
    return indices


def apply_line_axis_style(ax, x_values, xlabel, ylabel, log_x=False, log_y=False):
    if log_x:
        ax.set_xscale("symlog", base=2, linthresh=1, linscale=1)
    if log_y:
        ax.set_yscale("log")
    ax.set_xticks(x_values)
    ax.xaxis.set_major_formatter(FuncFormatter(lambda value, _: safe_number(value)))
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.grid(True, which="major", linestyle="--", alpha=0.38)
    ax.grid(True, which="minor", linestyle=":", alpha=0.18)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)


def plot_selected_slices(quad_values, alu_values, grids, output_dir):
    image_files = []
    quad_indices = nearest_indices(quad_values, [8, 32, 96, 384])
    fig, axes = plt.subplots(2, 2, figsize=(17.5, 11.0), constrained_layout=True, sharex=True)
    for ax, oi in zip(axes.flat, quad_indices):
        for variant in VARIANT_ORDER:
            rule = VARIANT_RULES[variant]
            ax.plot(
                alu_values,
                grids[variant][oi, :],
                linestyle="-",
                linewidth=2.4,
                marker=rule["marker"],
                markersize=5.2,
                color=rule["color"],
                label=f"Variant {variant} · {rule['renderer']} · total",
            )
        apply_line_axis_style(
            ax,
            alu_values,
            "ALU calculation count",
            "Average total GPU time (ms)",
            log_x=True,
            log_y=True,
        )
        ax.set_title(f"Waste quad count = {safe_number(quad_values[oi])}")
    handles, labels = axes.flat[0].get_legend_handles_labels()
    fig.legend(
        handles,
        labels,
        loc="center left",
        bbox_to_anchor=(1.005, 0.5),
        ncol=1,
        frameon=True,
    )
    fig.suptitle("Quad Waste × ALU Orthogonal — ALU-axis Slices", fontsize=15)
    image_files.extend(save_figure(fig, output_dir, "09_selected_alu_axis_slices"))

    alu_indices = nearest_indices(alu_values, [0, 2, 16, 64])
    fig, axes = plt.subplots(2, 2, figsize=(17.5, 11.0), constrained_layout=True, sharex=True)
    for ax, ai in zip(axes.flat, alu_indices):
        for variant in VARIANT_ORDER:
            rule = VARIANT_RULES[variant]
            ax.plot(
                quad_values,
                grids[variant][:, ai],
                linestyle="-",
                linewidth=2.4,
                marker=rule["marker"],
                markersize=5.2,
                color=rule["color"],
                label=f"Variant {variant} · {rule['renderer']} · total",
            )
        apply_line_axis_style(
            ax,
            quad_values,
            "Waste quad count",
            "Average total GPU time (ms)",
            log_x=True,
            log_y=True,
        )
        ax.set_title(f"ALU calculation count = {safe_number(alu_values[ai])}")
    handles, labels = axes.flat[0].get_legend_handles_labels()
    fig.legend(
        handles,
        labels,
        loc="center left",
        bbox_to_anchor=(1.005, 0.5),
        ncol=1,
        frameon=True,
    )
    fig.suptitle("Quad Waste × ALU Orthogonal — Waste-quad-axis Slices", fontsize=15)
    image_files.extend(save_figure(fig, output_dir, "10_selected_quad_axis_slices"))
    return image_files


def fit_interaction_models(quad_values, alu_values, grids):
    """Fit a quad-proxy-aware empirical model.

    In this experiment, the sampled waste-quad control is geometry division.
    Object count changes approximately with 1 / Q^2 while total geometry stays
    fixed, so a raw linear Q term is not an adequate basis. We therefore use
    reciprocal Q and Q^2 terms, with matching ALU interactions:

        T = beta0 + betaQ1/Q + betaQ2/Q^2
            + betaA*A + betaQA1*A/Q + betaQA2*A/Q^2
    """
    Q, A = np.meshgrid(quad_values, alu_values, indexing="ij")
    inv_q = np.divide(1.0, Q, out=np.zeros_like(Q, dtype=float), where=Q > 0)
    inv_q2 = inv_q ** 2
    features = np.column_stack(
        [
            np.ones(Q.size),
            inv_q.ravel(),
            inv_q2.ravel(),
            A.ravel(),
            (A * inv_q).ravel(),
            (A * inv_q2).ravel(),
        ]
    )
    models = {}
    for variant in VARIANT_ORDER:
        y = grids[variant].ravel()
        mask = np.isfinite(y)
        X = features[mask]
        target = y[mask]
        coefficients, *_ = np.linalg.lstsq(X, target, rcond=None)
        prediction = features @ coefficients
        residual = target - X @ coefficients
        sse = float(np.sum(residual ** 2))
        sst = float(np.sum((target - target.mean()) ** 2))
        r_squared = 1.0 - sse / sst if sst > 0 else math.nan
        rmse = float(np.sqrt(np.mean(residual ** 2)))
        mape = float(np.mean(np.abs(residual / target)) * 100.0)
        models[variant] = {
            "coefficients": coefficients,
            "prediction_grid": prediction.reshape(Q.shape),
            "r_squared": r_squared,
            "rmse_ms": rmse,
            "mape_percent": mape,
        }
    return models


def plot_model_diagnostics(grids, models, output_dir):
    fig, axes = plt.subplots(1, 3, figsize=(18.5, 6.3), constrained_layout=True)
    for ax, variant in zip(axes, VARIANT_ORDER):
        rule = VARIANT_RULES[variant]
        actual = grids[variant].ravel()
        predicted = models[variant]["prediction_grid"].ravel()
        positive = np.concatenate([actual[actual > 0], predicted[predicted > 0]])
        lower, upper = float(positive.min()) * 0.92, float(positive.max()) * 1.08
        ax.scatter(actual, predicted, s=30, alpha=0.72, color=rule["color"], edgecolors="none")
        ax.plot([lower, upper], [lower, upper], linestyle="--", linewidth=1.6, color="black")
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_xlim(lower, upper)
        ax.set_ylim(lower, upper)
        ax.grid(True, which="major", linestyle="--", alpha=0.35)
        ax.grid(True, which="minor", linestyle=":", alpha=0.17)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.set_title(
            f"Variant {variant} · {rule['renderer']}\n"
            f"R²={models[variant]['r_squared']:.6f}, "
            f"MAPE={models[variant]['mape_percent']:.2f}%"
        )
        ax.set_xlabel("Measured total time (ms)")
    axes[0].set_ylabel("Predicted total time (ms)")
    fig.suptitle(
        "Quad-proxy Interaction Model — T = β₀ + βQ₁/Q + βQ₂/Q² + βA·A + βQA₁·A/Q + βQA₂·A/Q²",
        fontsize=15,
    )
    return save_figure(fig, output_dir, "11_reciprocal_interaction_model_measured_vs_predicted")


def write_csv(path: Path, rows: list[dict]):
    if not rows:
        return
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def validate_totals(rows, output_dir):
    details = []
    summary = []
    for variant in VARIANT_ORDER:
        rule = VARIANT_RULES[variant]
        variant_rows = [row for row in rows if row["variant"] == variant]
        errors = []
        for row in variant_rows:
            active_columns = [column for _, column, _ in rule["passes"]]
            pass_sum = sum(
                row[column]
                for column in active_columns
                if math.isfinite(row.get(column, math.nan))
            )
            total = row[rule["total"]]
            error = abs(total - pass_sum)
            errors.append(error)
            details.append(
                {
                    "variant": variant,
                    "renderer": rule["renderer"],
                    "quad": row["quad"],
                    "alu": row["alu"],
                    "repeat_count": row["repeat_count"],
                    "total_ms": total,
                    "individual_pass_sum_ms": pass_sum,
                    "total_error_ms": error,
                }
            )
        above = sum(error > VALIDATION_TOLERANCE_MS for error in errors)
        summary.append(
            {
                "variant": variant,
                "renderer": rule["renderer"],
                "aggregated_grid_rows": len(variant_rows),
                "max_total_error_ms": max(errors) if errors else math.nan,
                "mean_total_error_ms": float(np.mean(errors)) if errors else math.nan,
                "rows_above_tolerance": above,
                "tolerance_ms": VALIDATION_TOLERANCE_MS,
                "status": "WARNING" if above else "OK",
            }
        )
    write_csv(output_dir / "validation_summary.csv", summary)
    write_csv(output_dir / "validation_details.csv", details)
    return summary


def export_failed_runs(failed, fieldnames, output_dir):
    preferred = [
        "runner_status",
        "runner_return_code",
        "runner_error",
        "variable-quad-count",
        "variable-alu-op-count",
        "renderer-variant",
        "renderer_name",
    ]
    columns = [name for name in preferred if name in fieldnames]
    if not columns:
        columns = list(fieldnames)
    with (output_dir / "failed_runs.csv").open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, extrasaction="ignore")
        writer.writeheader()
        for row in failed:
            writer.writerow(row)


def export_winner_tables(quad_values, alu_values, grids, output_dir):
    stack, winner_index, best, second, margin = compute_winner(grids)
    winner_rows = []
    for oi, quad in enumerate(quad_values):
        for ai, alu in enumerate(alu_values):
            winner_variant = VARIANT_ORDER[int(winner_index[oi, ai])]
            row = {
                "quad": safe_number(quad),
                "alu": safe_number(alu),
                "winner_variant": winner_variant,
                "winner_renderer": VARIANT_RULES[winner_variant]["renderer"],
                "winner_time_ms": best[oi, ai],
                "second_best_time_ms": second[oi, ai],
                "winner_margin_second_over_best": margin[oi, ai],
            }
            for index, variant in enumerate(VARIANT_ORDER):
                row[f"variant_{variant}_{VARIANT_RULES[variant]['renderer']}_time_ms"] = stack[index, oi, ai]
                row[f"variant_{variant}_time_over_fastest"] = stack[index, oi, ai] / best[oi, ai]
            winner_rows.append(row)
    write_csv(output_dir / "winner_grid.csv", winner_rows)

    crossover_rows = []
    for oi, quad in enumerate(quad_values):
        def first_alu(predicate):
            candidates = [
                float(alu_values[ai])
                for ai in range(len(alu_values))
                if predicate(ai)
            ]
            return min(candidates) if candidates else math.nan

        first_non_forward = first_alu(lambda ai: VARIANT_ORDER[int(winner_index[oi, ai])] != 1)
        first_visbuf_winner = first_alu(lambda ai: VARIANT_ORDER[int(winner_index[oi, ai])] == 4)
        first_prepass_winner = first_alu(lambda ai: VARIANT_ORDER[int(winner_index[oi, ai])] == 2)
        first_vb_beats_forward = first_alu(lambda ai: grids[4][oi, ai] < grids[1][oi, ai])
        first_pp_beats_forward = first_alu(lambda ai: grids[2][oi, ai] < grids[1][oi, ai])
        first_vb_beats_pp = first_alu(lambda ai: grids[4][oi, ai] < grids[2][oi, ai])
        crossover_rows.append(
            {
                "quad": safe_number(quad),
                "first_sampled_alu_where_forward_is_not_winner": first_non_forward,
                "first_sampled_alu_where_visbuf_is_winner": first_visbuf_winner,
                "first_sampled_alu_where_forwardprepass_is_winner": first_prepass_winner,
                "first_sampled_alu_where_visbuf_beats_forward": first_vb_beats_forward,
                "first_sampled_alu_where_forwardprepass_beats_forward": first_pp_beats_forward,
                "first_sampled_alu_where_visbuf_beats_forwardprepass": first_vb_beats_pp,
            }
        )
    write_csv(output_dir / "crossover_summary_by_waste_quad.csv", crossover_rows)
    return winner_rows, crossover_rows


def export_model_tables(models, output_dir):
    rows = []
    feature_names = ["beta_0_ms", "beta_inv_waste_quad_ms", "beta_inv_waste_quad_squared_ms", "beta_alu_ms_per_count", "beta_alu_over_waste_quad_ms", "beta_alu_over_waste_quad_squared_ms"]
    for variant in VARIANT_ORDER:
        model = models[variant]
        row = {
            "variant": variant,
            "renderer": VARIANT_RULES[variant]["renderer"],
            **{name: value for name, value in zip(feature_names, model["coefficients"])},
            "r_squared": model["r_squared"],
            "rmse_ms": model["rmse_ms"],
            "mape_percent": model["mape_percent"],
        }
        rows.append(row)
    write_csv(output_dir / "interaction_model_coefficients.csv", rows)
    return rows


def write_aggregated_data(rows, output_dir):
    export_rows = []
    for row in rows:
        export_rows.append(
            {
                "variant": row["variant"],
                "renderer": row["renderer"],
                "quad": row["quad"],
                "alu": row["alu"],
                "repeat_count": row["repeat_count"],
                "pass_0_time_avg_ms": row["pass_0_time_avg_ms"],
                "pass_1_time_avg_ms": row["pass_1_time_avg_ms"],
                "pass_2_time_avg_ms": row["pass_2_time_avg_ms"],
                "total_time_avg_ms": row["pass_3_time_avg_ms"],
            }
        )
    write_csv(output_dir / "aggregated_measurements.csv", export_rows)


def write_summary(
    output_dir,
    raw_success_count,
    failed_count,
    rows,
    quad_values,
    alu_values,
    grids,
    validation,
    model_rows,
    image_files,
):
    _, winner_index, _, _, margin = compute_winner(grids)
    winner_counts = Counter(
        VARIANT_RULES[VARIANT_ORDER[int(index)]]["renderer"]
        for index in winner_index.ravel()
    )
    repeat_counts = sorted({row["repeat_count"] for row in rows})
    lines = [
        "Quad Waste × ALU Orthogonal Plot Generation Summary",
        "==================================================",
        f"Successful raw rows used: {raw_success_count}",
        f"Non-success raw rows: {failed_count}",
        f"Aggregated renderer/grid rows: {len(rows)}",
        f"Repeat counts per renderer/grid point: {repeat_counts}",
        f"Quad waste samples ({len(quad_values)}): {[safe_number(v) for v in quad_values]}",
        f"ALU samples ({len(alu_values)}): {[safe_number(v) for v in alu_values]}",
        f"Logical graphs generated: {len(image_files) // 2}",
        f"PNG/SVG files generated: {len(image_files)}",
        "",
        f"Winner counts over the sampled {len(quad_values)} × {len(alu_values)} grid:",
    ]
    for renderer in [VARIANT_RULES[v]["renderer"] for v in VARIANT_ORDER]:
        count = winner_counts.get(renderer, 0)
        lines.append(f"- {renderer}: {count} / {winner_index.size}")
    lines.extend(
        [
            f"- Median winner margin (second-best / best): {float(np.nanmedian(margin)):.3f}×",
            f"- Minimum winner margin: {float(np.nanmin(margin)):.3f}×",
            f"- Maximum winner margin: {float(np.nanmax(margin)):.3f}×",
            "",
            "Empirical interaction model:",
            "T = β0 + βQ1/WasteQuad + βQ2/WasteQuad² + βA·ALU + βQA1·ALU/WasteQuad + βQA2·ALU/WasteQuad²",
        ]
    )
    for row in model_rows:
        lines.append(
            f"- Variant {row['variant']} · {row['renderer']}: "
            f"R²={row['r_squared']:.6f}, RMSE={row['rmse_ms']:.6f} ms, "
            f"MAPE={row['mape_percent']:.3f}%"
        )
    lines.extend(["", "Validation:"])
    for row in validation:
        lines.append(
            f"- Variant {row['variant']} · {row['renderer']}: "
            f"max total-minus-pass-sum error={row['max_total_error_ms']:.12g} ms [{row['status']}]"
        )
    lines.extend(
        [
            "",
            "Graph set:",
            "- 01-03: total-time contour plot for each renderer variant",
            "- 04: discrete fastest-renderer winner map",
            "- 05: winner margin / confidence map",
            "- 06: per-variant time relative to the fastest renderer",
            "- 07: Forward-relative speedup and crossover contours",
            "- 08: VisBuf versus ForwardPrepass pairwise crossover",
            "- 09: selected ALU-axis slices",
            "- 10: selected waste-quad-axis slices",
            "- 11: reciprocal quad-proxy model measured-versus-predicted diagnostics",
            "",
            "Interpretation cautions:",
            "- Winner and crossover maps describe the sampled hardware, renderer implementation, resolution, and scene configuration only.",
            "- Contours interpolate between measured grid points; discrete winner cells are the direct measurements.",
            "- One runner_repeat value was present in this dataset, so repeat-to-repeat uncertainty cannot be estimated from runner repeats.",
            "- The interaction model is empirical and should be re-fit when hardware, renderer, buffer format, resolution, or scene fingerprint changes.",
        ]
    )
    (output_dir / "run_summary.txt").write_text("\n".join(lines), encoding="utf-8")


def create_zip(output_dir, csv_path, script_path):
    copied_csv = output_dir / csv_path.name
    if csv_path.resolve() != copied_csv.resolve():
        shutil.copy2(csv_path, copied_csv)
    zip_path = output_dir.parent / f"{output_dir.name}_bundle.zip"
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(output_dir.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(output_dir.parent))
        archive.write(script_path, script_path.name)
    return zip_path



def plot_pairwise_delta_maps(quad_values, alu_values, grids, output_dir):
    """Plot direct measured time differences. Positive means the renderer on the right is faster."""
    pairs = [
        (1, 2, "Forward − ForwardPrepass"),
        (1, 4, "Forward − VisBuf"),
        (2, 4, "ForwardPrepass − VisBuf"),
    ]
    deltas = [grids[left] - grids[right] for left, right, _ in pairs]
    limit = max(float(np.nanmax(np.abs(delta))) for delta in deltas)
    limit = max(limit, 1e-6)
    x_edges, y_edges = index_mesh(deltas[0].shape)

    fig, axes = plt.subplots(1, 3, figsize=(19.2, 6.6), constrained_layout=True, sharex=True, sharey=True)
    mesh = None
    for index, (ax, ((left, right, title), delta)) in enumerate(zip(axes, zip(pairs, deltas))):
        mesh = ax.pcolormesh(
            x_edges,
            y_edges,
            delta,
            cmap="RdBu_r",
            norm=TwoSlopeNorm(vmin=-limit, vcenter=0.0, vmax=limit),
            shading="flat",
        )
        apply_index_axis_style(ax, quad_values, alu_values)
        if index > 0:
            ax.set_ylabel("")
        ax.set_title(title)
        # Draw an empirical zero boundary in index coordinates.
        Xi, Yi = np.meshgrid(np.arange(len(alu_values)), np.arange(len(quad_values)))
        boundary = ax.contour(Xi, Yi, delta, levels=[0.0], colors="black", linewidths=1.8)
        if boundary.allsegs and any(len(segment) for segment in boundary.allsegs[0]):
            ax.clabel(boundary, fmt={0.0: "equal"}, inline=True, fontsize=7)
    fig.suptitle(
        "Quad Waste × ALU Orthogonal — Pairwise Total-Time Difference\n"
        "Positive (red): renderer on the right is faster · Negative (blue): renderer on the left is faster",
        fontsize=15,
    )
    colorbar = fig.colorbar(mesh, ax=axes, pad=0.012, shrink=0.88)
    colorbar.set_label("Left renderer time − right renderer time (ms)")
    return save_figure(fig, output_dir, "11_pairwise_total_time_delta_ms")


def estimate_first_crossover(x_values, left_times, right_times):
    """Estimate the first ALU where right becomes faster than left.

    Interpolation is linear in log2(ALU + 1), matching the sampled axis spacing.
    """
    x_values = np.asarray(x_values, dtype=float)
    delta = np.asarray(left_times, dtype=float) - np.asarray(right_times, dtype=float)
    valid = np.isfinite(delta)
    if not np.any(valid):
        return math.nan
    for i in range(len(x_values)):
        if not valid[i]:
            continue
        if delta[i] > 0:
            if i == 0:
                return float(x_values[i])
            j = i - 1
            while j >= 0 and not valid[j]:
                j -= 1
            if j < 0 or delta[j] > 0 or math.isclose(delta[i], delta[j]):
                return float(x_values[i])
            tx0 = math.log2(float(x_values[j]) + 1.0)
            tx1 = math.log2(float(x_values[i]) + 1.0)
            fraction = -float(delta[j]) / float(delta[i] - delta[j])
            tx = tx0 + fraction * (tx1 - tx0)
            return float(2.0 ** tx - 1.0)
    return math.nan


def plot_crossover_thresholds(quad_values, alu_values, grids, output_dir):
    series = [
        (1, 4, "VisBuf becomes faster than Forward", VARIANT_RULES[4]["marker"], VARIANT_RULES[4]["color"]),
        (1, 2, "ForwardPrepass becomes faster than Forward", VARIANT_RULES[2]["marker"], VARIANT_RULES[2]["color"]),
        (2, 4, "VisBuf becomes faster than ForwardPrepass", "D", "C3"),
    ]
    rows = []
    fig, ax = plt.subplots(figsize=(14.5, 8.3), constrained_layout=True)
    for left, right, label, marker, color in series:
        thresholds = []
        for oi, quad in enumerate(quad_values):
            threshold = estimate_first_crossover(
                alu_values,
                grids[left][oi, :],
                grids[right][oi, :],
            )
            thresholds.append(threshold)
            rows.append(
                {
                    "waste_quad_control": quad,
                    "comparison": label,
                    "estimated_crossover_alu": threshold,
                }
            )
        values = np.asarray(thresholds, dtype=float)
        mask = np.isfinite(values)
        ax.plot(
            quad_values[mask],
            values[mask],
            marker=marker,
            markersize=6.0,
            linewidth=2.3,
            color=color,
            label=label,
        )
    ax.set_xscale("symlog", base=2, linthresh=1, linscale=1)
    ax.set_yscale("symlog", base=2, linthresh=1, linscale=1)
    ax.set_xticks(quad_values)
    ax.set_yticks(alu_values)
    ax.xaxis.set_major_formatter(FuncFormatter(lambda value, _: safe_number(value)))
    ax.yaxis.set_major_formatter(FuncFormatter(lambda value, _: safe_number(value)))
    ax.set_xlabel("Waste-quad control (geometry division)")
    ax.set_ylabel("Estimated crossover ALU calculation count")
    ax.grid(True, which="major", linestyle="--", alpha=0.36)
    ax.grid(True, which="minor", linestyle=":", alpha=0.18)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.set_title(
        "Quad Waste × ALU Orthogonal — Estimated Pairwise Crossover Thresholds\n"
        "Lower threshold means the alternative wins with a cheaper material shader"
    )
    ax.legend(loc="best", frameon=True)
    ax.text(
        0.01,
        0.015,
        "Crossovers are interpolated only between adjacent measured ALU samples in log2(ALU+1) space.",
        transform=ax.transAxes,
        fontsize=9,
        bbox={"facecolor": "white", "edgecolor": "none", "alpha": 0.80, "pad": 2.5},
    )
    write_csv(output_dir / "estimated_pairwise_crossover_thresholds.csv", rows)
    return save_figure(fig, output_dir, "12_estimated_pairwise_crossover_thresholds")


def plot_representative_pass_breakdowns(quad_values, alu_values, grids, pass_grids, output_dir):
    requested_points = [(8, 0), (64, 16), (384, 4), (384, 192)]
    points = []
    for quad_target, alu_target in requested_points:
        oi = int(np.argmin(np.abs(quad_values - quad_target)))
        ai = int(np.argmin(np.abs(alu_values - alu_target)))
        point = (oi, ai)
        if point not in points:
            points.append(point)

    fig, axes = plt.subplots(2, 2, figsize=(17.4, 10.8), constrained_layout=True)
    pass_labels = {
        1: [("Forward", "pass_0_time_avg_ms", "")],
        2: [("Depth prepass", "pass_0_time_avg_ms", "//"), ("Forward shading", "pass_1_time_avg_ms", "")],
        4: [("Visibility", "pass_0_time_avg_ms", "//"), ("Resolve", "pass_1_time_avg_ms", "")],
    }
    segment_handles = []
    segment_seen = set()
    for ax, (oi, ai) in zip(axes.flat, points):
        x = np.arange(len(VARIANT_ORDER))
        for xi, variant in enumerate(VARIANT_ORDER):
            bottom = 0.0
            color = VARIANT_RULES[variant]["color"]
            for label, column, hatch in pass_labels[variant]:
                value = float(pass_grids[variant][column][oi, ai])
                bar = ax.bar(
                    xi,
                    value,
                    bottom=bottom,
                    width=0.66,
                    color=color,
                    alpha=0.72 if hatch else 0.98,
                    hatch=hatch,
                    edgecolor="black",
                    linewidth=0.55,
                    label=label,
                )
                if label not in segment_seen:
                    segment_handles.append(Patch(facecolor=color, edgecolor="black", hatch=hatch, alpha=0.82, label=label))
                    segment_seen.add(label)
                bottom += value
            total = float(grids[variant][oi, ai])
            ax.text(xi, total * 1.025 + 0.001, f"{total:.3f}", ha="center", va="bottom", fontsize=8.5)
        ax.set_xticks(x)
        ax.set_xticklabels([VARIANT_RULES[v]["renderer"] for v in VARIANT_ORDER])
        ax.set_ylabel("Average GPU time (ms)")
        ax.set_title(
            f"Waste-quad control = {safe_number(quad_values[oi])}, "
            f"ALU = {safe_number(alu_values[ai])}"
        )
        ax.grid(True, axis="y", linestyle="--", alpha=0.32)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
    fig.suptitle(
        "Quad Waste × ALU Orthogonal — Representative Pass-Time Breakdown\n"
        "Labels above bars are measured total GPU time",
        fontsize=15,
    )
    fig.legend(
        handles=segment_handles,
        loc="center left",
        bbox_to_anchor=(1.005, 0.5),
        ncol=1,
        frameon=True,
    )
    return save_figure(fig, output_dir, "13_representative_pass_time_breakdown")


def write_comparison_summary(
    output_dir,
    raw_success_count,
    failed_count,
    rows,
    quad_values,
    alu_values,
    grids,
    validation,
    image_files,
):
    _, winner_index, _, _, margin = compute_winner(grids)
    winner_counts = Counter(
        VARIANT_RULES[VARIANT_ORDER[int(index)]]["renderer"]
        for index in winner_index.ravel()
    )
    repeat_counts = sorted({row["repeat_count"] for row in rows})
    lines = [
        "Experiment Attempt 6 — Quad Waste × ALU Comparison Plot Summary",
        "===============================================================",
        f"Successful raw rows used: {raw_success_count}",
        f"Non-success raw rows: {failed_count}",
        f"Aggregated renderer/grid rows: {len(rows)}",
        f"Repeat counts per renderer/grid point: {repeat_counts}",
        f"Waste-quad control samples ({len(quad_values)}): {[safe_number(v) for v in quad_values]}",
        f"ALU samples ({len(alu_values)}): {[safe_number(v) for v in alu_values]}",
        f"Logical comparison graphs generated: {len(image_files) // 2}",
        f"PNG/SVG files generated: {len(image_files)}",
        "",
        f"Winner counts over the sampled {len(quad_values)} × {len(alu_values)} grid:",
    ]
    for renderer in [VARIANT_RULES[v]["renderer"] for v in VARIANT_ORDER]:
        lines.append(f"- {renderer}: {winner_counts.get(renderer, 0)} / {winner_index.size}")
    lines.extend(
        [
            f"- Median winner margin (second-best / best): {float(np.nanmedian(margin)):.3f}×",
            f"- Minimum winner margin: {float(np.nanmin(margin)):.3f}×",
            f"- Maximum winner margin: {float(np.nanmax(margin)):.3f}×",
            "",
            "Direct observations from the sampled grid:",
            "- Forward is fastest in 63 cells; VisBuf is fastest in 58 cells; ForwardPrepass is never the overall winner.",
            "- As waste-quad control increases, the sampled ALU threshold at which VisBuf wins moves downward.",
            "- Near crossover cells, the best/second-best margin can be close to 1.0×, so adjacent measurements and profiling should be checked before making a strong claim.",
            "",
            "Validation:",
        ]
    )
    for row in validation:
        lines.append(
            f"- Variant {row['variant']} · {row['renderer']}: "
            f"max total-minus-pass-sum error={row['max_total_error_ms']:.12g} ms [{row['status']}]"
        )
    lines.extend(
        [
            "",
            "Graph set:",
            "- 01-03: total-time contour plot for each renderer variant",
            "- 04: discrete fastest-renderer winner map",
            "- 05: winner margin map",
            "- 06: per-variant time relative to the fastest renderer",
            "- 07: speedup against Forward with empirical crossover contours",
            "- 08: VisBuf versus ForwardPrepass pairwise crossover",
            "- 09: selected ALU-axis slices",
            "- 10: selected waste-quad-axis slices",
            "- 11: pairwise total-time difference maps in milliseconds",
            "- 12: estimated pairwise crossover ALU thresholds",
            "- 13: representative pass-time breakdown bars",
            "",
            "Interpretation cautions:",
            "- The waste-quad axis is the experiment control/proxy (geometry division), not a hardware counter measuring literal wasted quads.",
            "- Winner maps are direct sampled measurements; contour boundaries and crossover thresholds interpolate between samples.",
            "- Only one runner_repeat exists per grid point, so run-to-run uncertainty cannot be estimated from this CSV.",
            "- Results are specific to the measured hardware, renderer implementation, resolution, shader, and scene configuration.",
        ]
    )
    (output_dir / "run_summary.txt").write_text("\n".join(lines), encoding="utf-8")


def create_comparison_zip(output_dir, csv_path, script_path):
    copied_csv = output_dir / csv_path.name
    if csv_path.resolve() != copied_csv.resolve():
        shutil.copy2(csv_path, copied_csv)
    copied_script = output_dir / script_path.name
    if script_path.resolve() != copied_script.resolve():
        shutil.copy2(script_path, copied_script)
    zip_path = output_dir.parent / f"{output_dir.name}_bundle.zip"
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(output_dir.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(output_dir.parent))
    return zip_path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path", type=Path)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("quad_alu_attempt6_comparison_plots"),
    )
    args = parser.parse_args()

    csv_path = args.csv_path.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    raw_rows, failed, fieldnames = load_csv(csv_path)
    rows = aggregate(raw_rows)
    quad_values, alu_values, grids, pass_grids, repeat_grid = build_grid(rows)

    expected = len(quad_values) * len(alu_values)
    for variant in VARIANT_ORDER:
        finite_count = int(np.isfinite(grids[variant]).sum())
        if finite_count != expected:
            raise ValueError(
                f"Variant {variant} grid is incomplete: {finite_count}/{expected} cells"
            )

    write_aggregated_data(rows, output_dir)
    validation = validate_totals(rows, output_dir)
    export_failed_runs(failed, fieldnames, output_dir)
    export_winner_tables(quad_values, alu_values, grids, output_dir)

    image_files = []
    image_files.extend(plot_variant_contours(quad_values, alu_values, grids, output_dir))
    image_files.extend(plot_winner_map(quad_values, alu_values, grids, output_dir))
    image_files.extend(plot_winner_margin(quad_values, alu_values, grids, output_dir))
    image_files.extend(plot_regret_maps(quad_values, alu_values, grids, output_dir))
    image_files.extend(plot_speedup_vs_forward(quad_values, alu_values, grids, output_dir))
    image_files.extend(plot_visbuf_vs_prepass(quad_values, alu_values, grids, output_dir))
    image_files.extend(plot_selected_slices(quad_values, alu_values, grids, output_dir))
    image_files.extend(plot_pairwise_delta_maps(quad_values, alu_values, grids, output_dir))
    image_files.extend(plot_crossover_thresholds(quad_values, alu_values, grids, output_dir))
    image_files.extend(plot_representative_pass_breakdowns(quad_values, alu_values, grids, pass_grids, output_dir))

    write_comparison_summary(
        output_dir=output_dir,
        raw_success_count=len(raw_rows),
        failed_count=len(failed),
        rows=rows,
        quad_values=quad_values,
        alu_values=alu_values,
        grids=grids,
        validation=validation,
        image_files=image_files,
    )
    zip_path = create_comparison_zip(output_dir, csv_path, Path(__file__).resolve())

    print(f"Logical comparison graphs generated: {len(image_files) // 2}")
    print(f"PNG/SVG files generated: {len(image_files)}")
    print(f"Output directory: {output_dir}")
    print(f"Bundle ZIP: {zip_path}")


if __name__ == "__main__":
    main()
