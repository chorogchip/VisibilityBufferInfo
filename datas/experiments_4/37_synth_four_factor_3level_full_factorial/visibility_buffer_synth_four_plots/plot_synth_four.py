#!/usr/bin/env python3
"""Plot experiments 37/38 from VisibilityBufferInfo.

The script accepts the uploaded ZIP archives directly or extracted CSV files.
It produces publication-ready PNG/SVG figures plus analysis-ready CSV summaries.

Usage:
    python plot_synth_four.py \
        37_synth_four_factor_3level_full_factorial.zip \
        38_synth_four_one_dimensional_dense_sweeps.zip \
        --output-dir synth_four_plots
"""

from __future__ import annotations

import argparse
import io
import json
import math
import zipfile
from itertools import combinations
from pathlib import Path
from typing import Iterable

import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
import numpy as np
import pandas as pd


FACTORS = [
    "material-assign-max-open",
    "geometry-div",
    "material-assign-locality",
    "material-assign-diversity",
]

FACTOR_LABELS = {
    "material-assign-max-open": "Material-bin max open",
    "geometry-div": "Geometry division",
    "material-assign-locality": "Assignment locality",
    "material-assign-diversity": "Assignment diversity",
}

FACTOR_SHORT = {
    "material-assign-max-open": "Max open",
    "geometry-div": "Geometry div",
    "material-assign-locality": "Locality",
    "material-assign-diversity": "Diversity",
}

RENDERER_LABELS = {
    "DonutDeferredPrepass": "Deferred + prepass (variant 8)",
    "DonutVisGBuffer": "Visibility + G-buffer (variant 9)",
}

RENDERER_ORDER = ["DonutDeferredPrepass", "DonutVisGBuffer"]

REQUIRED_COLUMNS = {
    "renderer_name",
    "renderer-variant",
    "runner_status",
    "variable",
    "total_time_avg_ms",
    *FACTORS,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("factorial_input", type=Path, help="Experiment 37 ZIP/CSV/directory")
    parser.add_argument("dense_input", type=Path, help="Experiment 38 ZIP/CSV/directory")
    parser.add_argument("--output-dir", type=Path, default=Path("synth_four_plots"))
    parser.add_argument("--dpi", type=int, default=200)
    return parser.parse_args()


def read_csv_input(path: Path) -> pd.DataFrame:
    """Read the single benchmark CSV from a CSV, ZIP, or result directory."""
    if not path.exists():
        raise FileNotFoundError(path)

    if path.is_dir():
        csv_files = sorted(path.glob("*.csv"))
        if len(csv_files) != 1:
            raise ValueError(f"Expected exactly one CSV in {path}; found {len(csv_files)}")
        return pd.read_csv(csv_files[0], encoding="utf-8-sig")

    if path.suffix.lower() == ".zip":
        with zipfile.ZipFile(path) as archive:
            csv_members = sorted(name for name in archive.namelist() if name.lower().endswith(".csv"))
            if len(csv_members) != 1:
                raise ValueError(f"Expected exactly one CSV in {path}; found {len(csv_members)}")
            with archive.open(csv_members[0]) as file:
                return pd.read_csv(io.BytesIO(file.read()), encoding="utf-8-sig")

    if path.suffix.lower() == ".csv":
        return pd.read_csv(path, encoding="utf-8-sig")

    raise ValueError(f"Unsupported input type: {path}")


def clean(df: pd.DataFrame, source: str) -> tuple[pd.DataFrame, pd.DataFrame]:
    missing = sorted(REQUIRED_COLUMNS - set(df.columns))
    if missing:
        raise ValueError(f"{source}: missing required columns: {', '.join(missing)}")

    success_mask = df["runner_status"].astype(str).str.lower().eq("success")
    success = df.loc[success_mask].copy()
    failed = df.loc[~success_mask].copy()

    numeric = [
        "renderer-variant",
        "variable",
        "total_time_avg_ms",
        "total_time_median_ms",
        *FACTORS,
        "source_material_count",
        "active_material_bin_count",
        "material_bin_compaction_ratio",
        "variable-geometry-count",
        "variable-waste-quad-count",
    ]
    for column in numeric:
        if column in success.columns:
            success[column] = pd.to_numeric(success[column], errors="coerce")

    success = success.dropna(subset=["renderer_name", "total_time_avg_ms", *FACTORS])
    present_renderers = set(success["renderer_name"].unique())
    missing_renderers = [r for r in RENDERER_ORDER if r not in present_renderers]
    if missing_renderers:
        raise ValueError(f"{source}: expected renderer(s) missing: {missing_renderers}")
    return success, failed


def infer_baseline(df: pd.DataFrame) -> dict[str, float]:
    """Infer the central fixed values from the most frequent value of each factor."""
    baseline: dict[str, float] = {}
    conditions = df.drop_duplicates(subset=["variable", *FACTORS])
    for factor in FACTORS:
        modes = conditions[factor].mode(dropna=True)
        if modes.empty:
            raise ValueError(f"Cannot infer baseline for {factor}")
        baseline[factor] = float(modes.iloc[0])
    return baseline


def dense_long(df: pd.DataFrame, baseline: dict[str, float]) -> pd.DataFrame:
    """Build one factor-at-a-time sweeps; repeated central points estimate repeatability."""
    pieces = []
    for factor in FACTORS:
        mask = np.ones(len(df), dtype=bool)
        for other in FACTORS:
            if other != factor:
                mask &= np.isclose(df[other].to_numpy(float), baseline[other])
        part = df.loc[mask].copy()
        part["sweep"] = factor
        part["x_value"] = part[factor].astype(float)
        pieces.append(part)
    return pd.concat(pieces, ignore_index=True)


def summarize_dense(dense: pd.DataFrame) -> pd.DataFrame:
    summary = (
        dense.groupby(["sweep", "x_value", "renderer_name"], as_index=False)
        .agg(
            total_time_avg_ms=("total_time_avg_ms", "mean"),
            total_time_min_ms=("total_time_avg_ms", "min"),
            total_time_max_ms=("total_time_avg_ms", "max"),
            observations=("total_time_avg_ms", "size"),
        )
    )
    return summary


def paired_dense(summary: pd.DataFrame) -> pd.DataFrame:
    wide = summary.pivot_table(
        index=["sweep", "x_value"], columns="renderer_name", values="total_time_avg_ms"
    ).reset_index()
    wide.columns.name = None
    wide["vis_to_deferred_ratio"] = wide["DonutVisGBuffer"] / wide["DonutDeferredPrepass"]
    wide["vis_overhead_ms"] = wide["DonutVisGBuffer"] - wide["DonutDeferredPrepass"]
    wide["vis_overhead_percent"] = (wide["vis_to_deferred_ratio"] - 1.0) * 100.0
    return wide


def paired_factorial(df: pd.DataFrame) -> pd.DataFrame:
    wide = df.pivot_table(
        index=FACTORS, columns="renderer_name", values="total_time_avg_ms", aggfunc="mean"
    ).reset_index()
    wide.columns.name = None
    wide["vis_to_deferred_ratio"] = wide["DonutVisGBuffer"] / wide["DonutDeferredPrepass"]
    wide["vis_overhead_ms"] = wide["DonutVisGBuffer"] - wide["DonutDeferredPrepass"]
    wide["vis_overhead_percent"] = (wide["vis_to_deferred_ratio"] - 1.0) * 100.0
    return wide


def factorial_main_effects(df: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for renderer in RENDERER_ORDER:
        sub = df[df["renderer_name"] == renderer]
        grand_mean = float(sub["total_time_avg_ms"].mean())
        for factor in FACTORS:
            grouped = sub.groupby(factor)["total_time_avg_ms"].agg(["mean", "std", "count"])
            for level, values in grouped.iterrows():
                rows.append(
                    {
                        "renderer_name": renderer,
                        "factor": factor,
                        "level": float(level),
                        "mean_total_time_ms": float(values["mean"]),
                        "std_total_time_ms": float(values["std"]),
                        "count": int(values["count"]),
                        "normalized_to_renderer_grand_mean": float(values["mean"] / grand_mean),
                    }
                )
    return pd.DataFrame(rows)


def effect_sizes(main_effects: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for (renderer, factor), group in main_effects.groupby(["renderer_name", "factor"]):
        means = group["mean_total_time_ms"]
        grand = float((means * group["count"]).sum() / group["count"].sum())
        rows.append(
            {
                "renderer_name": renderer,
                "factor": factor,
                "min_marginal_mean_ms": float(means.min()),
                "max_marginal_mean_ms": float(means.max()),
                "marginal_range_ms": float(means.max() - means.min()),
                "marginal_range_percent_of_mean": float((means.max() - means.min()) / grand * 100.0),
            }
        )
    return pd.DataFrame(rows)


def extract_passes(df: pd.DataFrame) -> pd.DataFrame:
    records: list[dict[str, object]] = []
    for index, row in df.iterrows():
        for pass_index in range(32):
            name = row.get(f"pass_name_{pass_index}")
            time_value = row.get(f"pass_{pass_index}_time_avg_ms")
            if isinstance(name, str) and name.strip() and pd.notna(time_value):
                records.append(
                    {
                        "row_index": index,
                        "renderer_name": row["renderer_name"],
                        "variable": row["variable"],
                        **{factor: row[factor] for factor in FACTORS},
                        "pass": name.strip(),
                        "pass_time_avg_ms": float(time_value),
                    }
                )
    return pd.DataFrame(records)


def geometry_pass_summary(df: pd.DataFrame, baseline: dict[str, float]) -> pd.DataFrame:
    mask = np.ones(len(df), dtype=bool)
    for factor in FACTORS:
        if factor != "geometry-div":
            mask &= np.isclose(df[factor].to_numpy(float), baseline[factor])
    subset = df.loc[mask].copy()
    passes = extract_passes(subset)
    passes = passes[passes["pass"] != "total"]
    pass_summary = (
        passes.groupby(["geometry-div", "renderer_name", "pass"], as_index=False)["pass_time_avg_ms"]
        .mean()
    )
    totals = (
        subset.groupby(["geometry-div", "renderer_name"], as_index=False)["total_time_avg_ms"]
        .mean()
        .rename(columns={"total_time_avg_ms": "pass_time_avg_ms"})
    )
    totals["pass"] = "total"
    return pd.concat([totals, pass_summary], ignore_index=True)


def baseline_breakdown(df: pd.DataFrame, baseline: dict[str, float]) -> pd.DataFrame:
    mask = np.ones(len(df), dtype=bool)
    for factor, value in baseline.items():
        mask &= np.isclose(df[factor].to_numpy(float), value)
    subset = df.loc[mask].copy()
    passes = extract_passes(subset)
    rows = []
    for renderer in RENDERER_ORDER:
        total = float(subset.loc[subset["renderer_name"] == renderer, "total_time_avg_ms"].mean())
        p = passes[(passes["renderer_name"] == renderer) & (passes["pass"] != "total")]
        means = p.groupby("pass")["pass_time_avg_ms"].mean()
        if renderer == "DonutDeferredPrepass":
            frontend_names = ["depth_prepass", "geometry"]
        else:
            frontend_names = [
                "visibility",
                "visutil_histogram",
                "visutil_prefix",
                "visutil_flatten",
                "gbuffer",
            ]
        frontend = float(means.reindex(frontend_names).fillna(0).sum())
        lighting = float(means.get("lighting", 0.0))
        tonemap = float(means.get("tonemap", 0.0))
        other = max(0.0, total - frontend - lighting - tonemap)
        for category, value in [
            ("Front-end / visibility pipeline", frontend),
            ("Lighting", lighting),
            ("Tonemap", tonemap),
            ("Unattributed / other", other),
        ]:
            rows.append({"renderer_name": renderer, "category": category, "time_ms": value})
    return pd.DataFrame(rows)


def assign_original_dense_sweep(df: pd.DataFrame) -> pd.DataFrame:
    """Recover the concatenated sweep membership used by experiment 38."""
    result = df.copy()
    conditions = (
        result.drop_duplicates(subset=["variable", *FACTORS])
        .sort_values("variable")
        [["variable", *FACTORS]]
    )
    expected_lengths = [int(conditions[factor].nunique()) for factor in FACTORS]
    if sum(expected_lengths) != len(conditions):
        raise ValueError(
            "Could not infer original dense-sweep segments from factor cardinalities: "
            f"lengths={expected_lengths}, conditions={len(conditions)}"
        )
    mapping: dict[float, str] = {}
    start = 0
    ordered_variables = conditions["variable"].tolist()
    for factor, length in zip(FACTORS, expected_lengths):
        for variable in ordered_variables[start : start + length]:
            mapping[variable] = factor
        start += length
    result["original_sweep"] = result["variable"].map(mapping)
    return result


def baseline_repeatability(df: pd.DataFrame, baseline: dict[str, float]) -> pd.DataFrame:
    original = assign_original_dense_sweep(df)
    mask = np.ones(len(original), dtype=bool)
    for factor, value in baseline.items():
        mask &= np.isclose(original[factor].to_numpy(float), value)
    result = original.loc[
        mask, ["original_sweep", "variable", "renderer_name", "total_time_avg_ms"]
    ].drop_duplicates()
    result = result.rename(columns={"original_sweep": "sweep"})
    result = result.sort_values(["renderer_name", "sweep", "variable"]).reset_index(drop=True)
    result["renderer_mean_ms"] = result.groupby("renderer_name")["total_time_avg_ms"].transform("mean")
    result["deviation_percent"] = (result["total_time_avg_ms"] / result["renderer_mean_ms"] - 1.0) * 100.0
    return result


def diagnostic_summary(df37: pd.DataFrame, df38: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for experiment, df in [("37_full_factorial", df37), ("38_dense_sweeps", df38)]:
        for column in [
            "source_material_count",
            "active_material_bin_count",
            "material_bin_compaction_ratio",
            "variable-geometry-count",
            "variable-waste-quad-count",
        ]:
            if column in df.columns:
                values = pd.to_numeric(df[column], errors="coerce").dropna()
                rows.append(
                    {
                        "experiment": experiment,
                        "metric": column,
                        "unique_values": int(values.nunique()),
                        "minimum": float(values.min()),
                        "maximum": float(values.max()),
                    }
                )
    return pd.DataFrame(rows)


def style_axis(ax: plt.Axes) -> None:
    ax.grid(True, alpha=0.25)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)


def save_figure(fig: plt.Figure, output_dir: Path, stem: str, dpi: int) -> None:
    fig.tight_layout()
    fig.savefig(output_dir / f"{stem}.png", dpi=dpi, bbox_inches="tight")
    fig.savefig(output_dir / f"{stem}.svg", bbox_inches="tight")
    plt.close(fig)


def plot_key_results(
    dense_summary: pd.DataFrame,
    dense_pairs: pd.DataFrame,
    effects: pd.DataFrame,
    breakdown: pd.DataFrame,
    output_dir: Path,
    dpi: int,
) -> None:
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    # Geometry response.
    ax = axes[0, 0]
    geom = dense_summary[dense_summary["sweep"] == "geometry-div"]
    for renderer in RENDERER_ORDER:
        g = geom[geom["renderer_name"] == renderer].sort_values("x_value")
        ax.plot(g["x_value"], g["total_time_avg_ms"], marker="o", label=RENDERER_LABELS[renderer])
    ax.set_title("Geometry is the only strong input effect")
    ax.set_xlabel("Geometry division")
    ax.set_ylabel("Average total GPU time (ms)")
    ax.legend(fontsize=8)
    style_axis(ax)

    # Ratio response.
    ax = axes[0, 1]
    ratio = dense_pairs[dense_pairs["sweep"] == "geometry-div"].sort_values("x_value")
    ax.plot(ratio["x_value"], ratio["vis_to_deferred_ratio"], marker="o")
    ax.axhline(1.0, linewidth=1)
    ax.set_title("Visibility/deferred gap narrows with geometry")
    ax.set_xlabel("Geometry division")
    ax.set_ylabel("Variant 9 / variant 8 total time")
    style_axis(ax)

    # Effect magnitudes.
    ax = axes[1, 0]
    factor_order = FACTORS[::-1]
    positions = np.arange(len(factor_order), dtype=float)
    width = 0.36
    for index, renderer in enumerate(RENDERER_ORDER):
        values = (
            effects[effects["renderer_name"] == renderer]
            .set_index("factor")
            .reindex(factor_order)["marginal_range_percent_of_mean"]
            .to_numpy()
        )
        ax.barh(positions + (index - 0.5) * width, values, height=width, label=RENDERER_LABELS[renderer])
    ax.set_yticks(positions, [FACTOR_SHORT[f] for f in factor_order])
    ax.set_xlabel("Marginal mean range (% of renderer mean)")
    ax.set_title("Full-factorial main-effect magnitude")
    ax.legend(fontsize=8)
    style_axis(ax)

    # Baseline breakdown.
    ax = axes[1, 1]
    renderers = RENDERER_ORDER
    categories = list(dict.fromkeys(breakdown["category"]))
    bottom = np.zeros(len(renderers))
    for category in categories:
        values = [
            float(
                breakdown[(breakdown["renderer_name"] == renderer) & (breakdown["category"] == category)][
                    "time_ms"
                ].iloc[0]
            )
            for renderer in renderers
        ]
        ax.bar(range(len(renderers)), values, bottom=bottom, label=category)
        bottom += np.asarray(values)
    ax.set_xticks(range(len(renderers)), ["Variant 8", "Variant 9"])
    ax.set_ylabel("Average total GPU time (ms)")
    ax.set_title("Central-condition total-time composition")
    ax.legend(fontsize=8)
    style_axis(ax)

    fig.suptitle("Synthetic four-factor experiment: key results", fontsize=16)
    save_figure(fig, output_dir, "00_key_results", dpi)


def configure_factor_x(ax: plt.Axes, factor: str, values: Iterable[float]) -> None:
    values = sorted(set(float(v) for v in values))
    if factor == "material-assign-max-open":
        ax.set_xscale("log", base=2)
        ticks = [1, 2, 4, 8, 16, 32, 64, 128, 255]
        ax.set_xticks([tick for tick in ticks if min(values) <= tick <= max(values)])
        ax.xaxis.set_major_formatter(FuncFormatter(lambda value, _: f"{value:g}"))
    elif factor in {"material-assign-locality", "material-assign-diversity"}:
        ax.set_xlim(-0.02, 1.02)
        ax.set_xticks(np.linspace(0, 1, 6))


def plot_dense_absolute(summary: pd.DataFrame, output_dir: Path, dpi: int) -> None:
    fig, axes = plt.subplots(2, 2, figsize=(14, 9))
    for ax, factor in zip(axes.flat, FACTORS):
        subset = summary[summary["sweep"] == factor]
        for renderer in RENDERER_ORDER:
            g = subset[subset["renderer_name"] == renderer].sort_values("x_value")
            ax.plot(g["x_value"], g["total_time_avg_ms"], marker="o", label=RENDERER_LABELS[renderer])
            repeated = g["observations"] > 1
            if repeated.any():
                ax.vlines(
                    g.loc[repeated, "x_value"],
                    g.loc[repeated, "total_time_min_ms"],
                    g.loc[repeated, "total_time_max_ms"],
                    linewidth=2,
                )
        configure_factor_x(ax, factor, subset["x_value"])
        ax.set_title(FACTOR_LABELS[factor])
        ax.set_xlabel(FACTOR_LABELS[factor])
        ax.set_ylabel("Average total GPU time (ms)")
        style_axis(ax)
    axes[0, 0].legend(fontsize=8)
    fig.suptitle("Dense one-dimensional sweeps — absolute total time", fontsize=16)
    save_figure(fig, output_dir, "01_dense_sweeps_absolute", dpi)


def plot_dense_normalized(
    summary: pd.DataFrame, baseline: dict[str, float], output_dir: Path, dpi: int
) -> None:
    fig, axes = plt.subplots(2, 2, figsize=(14, 9))
    all_normalized = []
    for factor in FACTORS:
        subset = summary[summary["sweep"] == factor]
        for renderer in RENDERER_ORDER:
            g = subset[subset["renderer_name"] == renderer].copy().sort_values("x_value")
            base_rows = g[np.isclose(g["x_value"], baseline[factor])]
            if base_rows.empty:
                base_value = float(g["total_time_avg_ms"].median())
            else:
                base_value = float(base_rows["total_time_avg_ms"].mean())
            all_normalized.extend((g["total_time_avg_ms"] / base_value).tolist())
    ymin = min(all_normalized)
    ymax = max(all_normalized)
    pad = (ymax - ymin) * 0.08 if ymax > ymin else 0.01

    for ax, factor in zip(axes.flat, FACTORS):
        subset = summary[summary["sweep"] == factor]
        for renderer in RENDERER_ORDER:
            g = subset[subset["renderer_name"] == renderer].copy().sort_values("x_value")
            base_value = float(
                g.loc[np.isclose(g["x_value"], baseline[factor]), "total_time_avg_ms"].mean()
            )
            ax.plot(
                g["x_value"],
                g["total_time_avg_ms"] / base_value,
                marker="o",
                label=RENDERER_LABELS[renderer],
            )
        ax.axhline(1.0, linewidth=1)
        configure_factor_x(ax, factor, subset["x_value"])
        ax.set_ylim(ymin - pad, ymax + pad)
        ax.set_title(FACTOR_LABELS[factor])
        ax.set_xlabel(FACTOR_LABELS[factor])
        ax.set_ylabel("Total time / central-condition time")
        style_axis(ax)
    axes[0, 0].legend(fontsize=8)
    fig.suptitle("Dense one-dimensional sweeps — normalized sensitivity", fontsize=16)
    save_figure(fig, output_dir, "02_dense_sweeps_normalized", dpi)


def plot_dense_ratio(pairs: pd.DataFrame, output_dir: Path, dpi: int) -> None:
    fig, axes = plt.subplots(2, 2, figsize=(14, 9))
    ymin = float(pairs["vis_to_deferred_ratio"].min())
    ymax = float(pairs["vis_to_deferred_ratio"].max())
    pad = (ymax - ymin) * 0.08
    for ax, factor in zip(axes.flat, FACTORS):
        g = pairs[pairs["sweep"] == factor].sort_values("x_value")
        ax.plot(g["x_value"], g["vis_to_deferred_ratio"], marker="o")
        configure_factor_x(ax, factor, g["x_value"])
        ax.set_ylim(ymin - pad, ymax + pad)
        ax.set_title(FACTOR_LABELS[factor])
        ax.set_xlabel(FACTOR_LABELS[factor])
        ax.set_ylabel("Variant 9 / variant 8 total time")
        style_axis(ax)
    fig.suptitle("Dense one-dimensional sweeps — renderer cost ratio", fontsize=16)
    save_figure(fig, output_dir, "03_dense_sweeps_renderer_ratio", dpi)


def plot_factorial_main_effects(main_effects: pd.DataFrame, output_dir: Path, dpi: int) -> None:
    fig, axes = plt.subplots(1, 4, figsize=(18, 4.7), sharey=True)
    ymin = float(main_effects["normalized_to_renderer_grand_mean"].min())
    ymax = float(main_effects["normalized_to_renderer_grand_mean"].max())
    pad = (ymax - ymin) * 0.10
    for ax, factor in zip(axes, FACTORS):
        subset = main_effects[main_effects["factor"] == factor]
        for renderer in RENDERER_ORDER:
            g = subset[subset["renderer_name"] == renderer].sort_values("level")
            ax.plot(
                g["level"],
                g["normalized_to_renderer_grand_mean"],
                marker="o",
                label=RENDERER_LABELS[renderer],
            )
        ax.axhline(1.0, linewidth=1)
        configure_factor_x(ax, factor, subset["level"])
        ax.set_ylim(ymin - pad, ymax + pad)
        ax.set_title(FACTOR_SHORT[factor])
        ax.set_xlabel(FACTOR_LABELS[factor])
        style_axis(ax)
    axes[0].set_ylabel("Marginal mean / renderer grand mean")
    axes[0].legend(fontsize=8)
    fig.suptitle("Three-level full factorial — main effects", fontsize=16)
    save_figure(fig, output_dir, "04_full_factorial_main_effects", dpi)


def plot_factorial_interactions(pairs: pd.DataFrame, output_dir: Path, dpi: int) -> None:
    pairs_list = list(combinations(FACTORS, 2))
    matrices = []
    for factor_x, factor_y in pairs_list:
        matrix = pairs.pivot_table(
            index=factor_y,
            columns=factor_x,
            values="vis_to_deferred_ratio",
            aggfunc="mean",
        ).sort_index().sort_index(axis=1)
        matrices.append(matrix)
    vmin = min(float(matrix.min().min()) for matrix in matrices)
    vmax = max(float(matrix.max().max()) for matrix in matrices)

    fig = plt.figure(figsize=(15, 9.5))
    grid = fig.add_gridspec(
        3, 3, height_ratios=[1.0, 1.0, 0.07], hspace=0.46, wspace=0.35
    )
    axes = np.array(
        [[fig.add_subplot(grid[row, column]) for column in range(3)] for row in range(2)]
    )
    colorbar_axis = fig.add_subplot(grid[2, :])

    image = None
    for ax, (factor_x, factor_y), matrix in zip(axes.flat, pairs_list, matrices):
        image = ax.imshow(matrix.to_numpy(), origin="lower", aspect="auto", vmin=vmin, vmax=vmax)
        ax.set_xticks(range(len(matrix.columns)), [f"{value:g}" for value in matrix.columns])
        ax.set_yticks(range(len(matrix.index)), [f"{value:g}" for value in matrix.index])
        ax.set_xlabel(FACTOR_SHORT[factor_x])
        ax.set_ylabel(FACTOR_SHORT[factor_y])
        ax.set_title(f"{FACTOR_SHORT[factor_y]} × {FACTOR_SHORT[factor_x]}")
        for row in range(matrix.shape[0]):
            for col in range(matrix.shape[1]):
                ax.text(col, row, f"{matrix.iloc[row, col]:.2f}", ha="center", va="center", fontsize=8)
    if image is not None:
        fig.colorbar(
            image,
            cax=colorbar_axis,
            label="Variant 9 / variant 8 total time",
            orientation="horizontal",
        )
    fig.suptitle("Three-level full factorial — pairwise renderer-ratio interactions", fontsize=16)
    fig.subplots_adjust(top=0.91, bottom=0.08)
    fig.savefig(output_dir / "05_full_factorial_pairwise_interactions.png", dpi=dpi, bbox_inches="tight")
    fig.savefig(output_dir / "05_full_factorial_pairwise_interactions.svg", bbox_inches="tight")
    plt.close(fig)

def plot_geometry_passes(summary: pd.DataFrame, output_dir: Path, dpi: int) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(16, 6))
    for ax, renderer in zip(axes, RENDERER_ORDER):
        subset = summary[summary["renderer_name"] == renderer]
        pass_order = ["total"] + sorted(p for p in subset["pass"].unique() if p != "total")
        for pass_name in pass_order:
            g = subset[subset["pass"] == pass_name].sort_values("geometry-div")
            if g.empty:
                continue
            if pass_name == "total":
                ax.plot(g["geometry-div"], g["pass_time_avg_ms"], marker="o", linewidth=2.8, label="total")
            else:
                ax.plot(g["geometry-div"], g["pass_time_avg_ms"], marker=".", linewidth=1.4, label=pass_name)
        ax.set_title(RENDERER_LABELS[renderer])
        ax.set_xlabel("Geometry division")
        ax.set_ylabel("Average pass time (ms)")
        ax.legend(fontsize=8, ncol=2)
        style_axis(ax)
    fig.suptitle("Dense geometry sweep — named pass timing", fontsize=16)
    save_figure(fig, output_dir, "06_geometry_pass_breakdown", dpi)


def plot_effect_sizes(effects: pd.DataFrame, output_dir: Path, dpi: int) -> None:
    fig, ax = plt.subplots(figsize=(10, 5.5))
    factor_order = FACTORS[::-1]
    positions = np.arange(len(factor_order), dtype=float)
    width = 0.36
    for index, renderer in enumerate(RENDERER_ORDER):
        values = (
            effects[effects["renderer_name"] == renderer]
            .set_index("factor")
            .reindex(factor_order)["marginal_range_percent_of_mean"]
            .to_numpy()
        )
        bars = ax.barh(
            positions + (index - 0.5) * width,
            values,
            height=width,
            label=RENDERER_LABELS[renderer],
        )
        ax.bar_label(bars, fmt="%.2f%%", padding=3, fontsize=8)
    ax.set_yticks(positions, [FACTOR_LABELS[f] for f in factor_order])
    ax.set_xlabel("Marginal mean range (% of renderer mean)")
    ax.set_title("Full-factorial factor sensitivity")
    ax.legend(fontsize=8)
    style_axis(ax)
    save_figure(fig, output_dir, "07_full_factorial_effect_sizes", dpi)


def plot_repeatability(repeatability: pd.DataFrame, output_dir: Path, dpi: int) -> None:
    fig, ax = plt.subplots(figsize=(11, 5.5))
    labels = [FACTOR_SHORT[f] for f in FACTORS]
    positions = np.arange(len(FACTORS), dtype=float)
    for renderer in RENDERER_ORDER:
        g = repeatability[repeatability["renderer_name"] == renderer]
        grouped = g.groupby("sweep")["deviation_percent"].mean().reindex(FACTORS)
        ax.plot(positions, grouped.to_numpy(), marker="o", label=RENDERER_LABELS[renderer])
        for factor_index, factor in enumerate(FACTORS):
            points = g[g["sweep"] == factor]["deviation_percent"].to_numpy()
            if len(points):
                jitter = np.linspace(-0.035, 0.035, len(points))
                ax.scatter(np.full(len(points), factor_index) + jitter, points, alpha=0.55, s=24)
    ax.axhline(0.0, linewidth=1)
    ax.set_xticks(positions, labels)
    ax.set_ylabel("Deviation from renderer baseline mean (%)")
    ax.set_title("Repeated central condition embedded in the four dense sweeps")
    ax.legend(fontsize=8)
    style_axis(ax)
    save_figure(fig, output_dir, "08_dense_baseline_repeatability", dpi)


def write_readme(
    output_dir: Path,
    df37: pd.DataFrame,
    df38: pd.DataFrame,
    baseline: dict[str, float],
    dense_pairs: pd.DataFrame,
    effects: pd.DataFrame,
    diagnostics: pd.DataFrame,
    repeatability: pd.DataFrame,
) -> None:
    center = dense_pairs.copy()
    center_mask = np.ones(len(center), dtype=bool)
    # The same central point appears in every sweep; average those four observations.
    for factor in FACTORS:
        center_mask &= np.where(center["sweep"].eq(factor), np.isclose(center["x_value"], baseline[factor]), True)
    # Easier and explicit: select each sweep's own baseline point.
    center_rows = pd.concat(
        [
            dense_pairs[(dense_pairs["sweep"] == factor) & np.isclose(dense_pairs["x_value"], baseline[factor])]
            for factor in FACTORS
        ],
        ignore_index=True,
    )
    deferred_center = float(center_rows["DonutDeferredPrepass"].mean())
    vis_center = float(center_rows["DonutVisGBuffer"].mean())
    ratio_center = vis_center / deferred_center

    geometry = dense_pairs[dense_pairs["sweep"] == "geometry-div"].sort_values("x_value")
    g0, g1 = geometry.iloc[0], geometry.iloc[-1]
    deferred_geometry_delta = (g1["DonutDeferredPrepass"] / g0["DonutDeferredPrepass"] - 1.0) * 100.0
    vis_geometry_delta = (g1["DonutVisGBuffer"] / g0["DonutVisGBuffer"] - 1.0) * 100.0

    effect_table = effects.pivot(index="factor", columns="renderer_name", values="marginal_range_percent_of_mean")
    material_metrics = diagnostics[diagnostics["metric"].isin(["source_material_count", "active_material_bin_count", "material_bin_compaction_ratio"])]
    material_constant = bool((material_metrics["unique_values"] == 1).all())

    repeat_range = repeatability.groupby("renderer_name")["deviation_percent"].agg(lambda s: float(s.max() - s.min()))

    max_open = dense_pairs[dense_pairs["sweep"] == "material-assign-max-open"].copy()
    deferred_reference = float(
        max_open.loc[~np.isclose(max_open["x_value"], 2.0), "DonutDeferredPrepass"].median()
    )
    deferred_at_two = max_open.loc[
        np.isclose(max_open["x_value"], 2.0), "DonutDeferredPrepass"
    ]
    max_open_outlier_percent = (
        float(deferred_at_two.iloc[0]) / deferred_reference - 1.0
    ) * 100.0 if not deferred_at_two.empty else math.nan

    lines = [
        "# VisibilityBufferInfo experiments 37/38 — plot bundle",
        "",
        "## 핵심 결과",
        "",
        f"- 총 {len(df37) + len(df38):,}개 성공 run을 사용했습니다: full factorial {len(df37):,}개, dense sweeps {len(df38):,}개.",
        f"- 중앙 조건에서 variant 8은 **{deferred_center:.5f} ms**, variant 9는 **{vis_center:.5f} ms**로, variant 9가 **{ratio_center:.3f}×**입니다.",
        f"- geometry division 8→128에서 variant 8은 **{deferred_geometry_delta:+.2f}%**, variant 9는 **{vis_geometry_delta:+.2f}%** 변했습니다. renderer 비율은 **{g0['vis_to_deferred_ratio']:.3f}× → {g1['vis_to_deferred_ratio']:.3f}×**로 감소합니다.",
        f"- full factorial의 주효과 범위는 geometry division이 가장 큽니다: variant 8 **{effect_table.loc['geometry-div', 'DonutDeferredPrepass']:.2f}%**, variant 9 **{effect_table.loc['geometry-div', 'DonutVisGBuffer']:.2f}%**.",
    ]
    if material_constant:
        lines.append(
            "- `source_material_count=1`, `active_material_bin_count=1`, `material_bin_compaction_ratio=1.0`이 모든 run에서 고정입니다. 따라서 max-open/locality/diversity의 평탄한 결과는 재료 bin 알고리즘의 무관성을 뜻하기보다, 현재 장면이 해당 축을 실제로 활성화하지 못했다는 진단으로 보는 편이 안전합니다."
        )
    lines.extend(
        [
            f"- dense sweep에 반복 삽입된 중앙 조건의 범위는 variant 8 **{repeat_range.get('DonutDeferredPrepass', math.nan):.3f}%p**, variant 9 **{repeat_range.get('DonutVisGBuffer', math.nan):.3f}%p**입니다. 각 비중앙 조건은 repeat=1이므로 작은 요동은 효과보다 측정 잡음일 수 있습니다.",
            f"- max-open=2의 variant 8 값은 나머지 max-open 지점 중앙값보다 **{max_open_outlier_percent:+.2f}%** 높아 고립된 이상점으로 보입니다. repeat=1이라 재실행 전에는 max-open 효과로 해석하지 않는 편이 안전합니다.",
            "",
            "## 파일 구성",
            "",
            "- `00_key_results`: 핵심 결과 4-panel 요약",
            "- `01_dense_sweeps_absolute`: 네 축의 절대 total time",
            "- `02_dense_sweeps_normalized`: 중앙 조건 대비 민감도",
            "- `03_dense_sweeps_renderer_ratio`: variant 9 / variant 8 비율",
            "- `04_full_factorial_main_effects`: 3수준 완전요인 주효과",
            "- `05_full_factorial_pairwise_interactions`: pairwise interaction heatmap",
            "- `06_geometry_pass_breakdown`: geometry sweep의 pass별 시간",
            "- `07_full_factorial_effect_sizes`: factor 민감도 순위",
            "- `08_dense_baseline_repeatability`: 네 sweep에 반복된 중앙 조건의 변동",
            "- `*.csv`: plotting에 사용한 정리된 수치",
            "- `plot_synth_four.py`: ZIP/CSV에서 전체 결과를 재생성하는 스크립트",
            "",
            "## 실행",
            "",
            "```bash",
            "python plot_synth_four.py 37_synth_four_factor_3level_full_factorial.zip 38_synth_four_one_dimensional_dense_sweeps.zip --output-dir synth_four_plots",
            "```",
            "",
            "PNG와 SVG를 모두 생성합니다.",
        ]
    )
    (output_dir / "README.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    raw37 = read_csv_input(args.factorial_input)
    raw38 = read_csv_input(args.dense_input)
    df37, failed37 = clean(raw37, "experiment 37")
    df38, failed38 = clean(raw38, "experiment 38")

    baseline = infer_baseline(df38)
    dense = dense_long(df38, baseline)
    dense_summary = summarize_dense(dense)
    dense_pairs = paired_dense(dense_summary)
    factorial_pairs = paired_factorial(df37)
    main_effects = factorial_main_effects(df37)
    effects = effect_sizes(main_effects)
    geometry_passes = geometry_pass_summary(df38, baseline)
    breakdown = baseline_breakdown(df38, baseline)
    repeatability = baseline_repeatability(df38, baseline)
    diagnostics = diagnostic_summary(df37, df38)

    # Machine-readable summaries.
    dense_summary.to_csv(args.output_dir / "dense_sweep_summary.csv", index=False)
    dense_pairs.to_csv(args.output_dir / "dense_sweep_paired.csv", index=False)
    factorial_pairs.to_csv(args.output_dir / "full_factorial_paired.csv", index=False)
    main_effects.to_csv(args.output_dir / "full_factorial_main_effects.csv", index=False)
    effects.to_csv(args.output_dir / "full_factorial_effect_sizes.csv", index=False)
    geometry_passes.to_csv(args.output_dir / "geometry_pass_summary.csv", index=False)
    breakdown.to_csv(args.output_dir / "baseline_pass_breakdown.csv", index=False)
    repeatability.to_csv(args.output_dir / "dense_baseline_repeatability.csv", index=False)
    diagnostics.to_csv(args.output_dir / "experiment_diagnostics.csv", index=False)
    pd.concat(
        [failed37.assign(experiment="37_full_factorial"), failed38.assign(experiment="38_dense_sweeps")],
        ignore_index=True,
    ).to_csv(args.output_dir / "failed_runs.csv", index=False)

    manifest = {
        "factorial_input": str(args.factorial_input),
        "dense_input": str(args.dense_input),
        "successful_runs": {"37_full_factorial": len(df37), "38_dense_sweeps": len(df38)},
        "failed_runs": {"37_full_factorial": len(failed37), "38_dense_sweeps": len(failed38)},
        "inferred_dense_baseline": baseline,
        "renderers": RENDERER_LABELS,
        "metric": "total_time_avg_ms",
    }
    (args.output_dir / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    # Figures.
    plot_key_results(dense_summary, dense_pairs, effects, breakdown, args.output_dir, args.dpi)
    plot_dense_absolute(dense_summary, args.output_dir, args.dpi)
    plot_dense_normalized(dense_summary, baseline, args.output_dir, args.dpi)
    plot_dense_ratio(dense_pairs, args.output_dir, args.dpi)
    plot_factorial_main_effects(main_effects, args.output_dir, args.dpi)
    plot_factorial_interactions(factorial_pairs, args.output_dir, args.dpi)
    plot_geometry_passes(geometry_passes, args.output_dir, args.dpi)
    plot_effect_sizes(effects, args.output_dir, args.dpi)
    plot_repeatability(repeatability, args.output_dir, args.dpi)
    write_readme(
        args.output_dir,
        df37,
        df38,
        baseline,
        dense_pairs,
        effects,
        diagnostics,
        repeatability,
    )

    print(f"Wrote plots and summaries to: {args.output_dir.resolve()}")


if __name__ == "__main__":
    main()
