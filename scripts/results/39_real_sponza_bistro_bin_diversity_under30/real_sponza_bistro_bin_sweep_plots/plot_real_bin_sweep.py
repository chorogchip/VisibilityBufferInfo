#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import shutil
import tempfile
import zipfile
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.colors import TwoSlopeNorm
import numpy as np
import pandas as pd

RENDERER_LABELS = {
    8: "Deferred + prepass",
    9: "Visibility + G-buffer",
}
SCENE_ORDER = ["Sponza", "Bistro"]
PASS_GROUPS_VIS = {
    "Visibility": ["visibility"],
    "Bin utilities": ["visutil_histogram", "visutil_prefix", "visutil_flatten"],
    "G-buffer": ["gbuffer"],
    "Lighting + tonemap": ["lighting", "tonemap"],
}


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Plot the real-scene material-bin sweep.")
    p.add_argument("input", type=Path, help="Experiment ZIP or extracted directory")
    p.add_argument("--output-dir", type=Path, default=Path("real_bin_sweep_plots"))
    return p.parse_args()


def scene_label(path: str) -> str:
    lower = str(path).lower()
    if "sponza" in lower:
        return "Sponza"
    if "bistro" in lower:
        return "Bistro"
    return Path(str(path)).stem or "Scene"


def prepare_input(input_path: Path) -> tuple[Path, tempfile.TemporaryDirectory | None]:
    if input_path.is_dir():
        return input_path, None
    if input_path.suffix.lower() != ".zip":
        raise ValueError("Input must be a ZIP file or an extracted directory")
    temp = tempfile.TemporaryDirectory(prefix="real_bin_sweep_")
    with zipfile.ZipFile(input_path) as zf:
        zf.extractall(temp.name)
    return Path(temp.name), temp


def find_main_csv(root: Path) -> Path:
    candidates = [p for p in root.glob("*.csv") if "run_" not in p.name]
    if len(candidates) != 1:
        raise FileNotFoundError(f"Expected one main CSV in {root}, found {len(candidates)}")
    return candidates[0]


def validate_and_prepare(main_csv: Path) -> pd.DataFrame:
    df = pd.read_csv(main_csv)
    required = [
        "runner_status", "runner_run_index", "renderer-variant", "renderer_name",
        "scene-path", "material-assign-max-open", "material-assign-diversity",
        "source_material_count", "active_material_bin_count",
        "material_bin_compaction_ratio", "total_time_avg_ms",
    ]
    missing = [c for c in required if c not in df.columns]
    if missing:
        raise KeyError(f"Missing required columns: {missing}")
    failed = df[~df["runner_status"].astype(str).str.lower().eq("success")]
    if len(failed):
        raise ValueError(f"Experiment contains {len(failed)} non-success rows")
    df = df.copy()
    df["renderer_variant"] = pd.to_numeric(df["renderer-variant"]).astype(int)
    df["renderer"] = df["renderer_variant"].map(RENDERER_LABELS).fillna(df["renderer_name"])
    df["scene"] = df["scene-path"].map(scene_label)
    df["max_open"] = pd.to_numeric(df["material-assign-max-open"]).astype(int)
    df["diversity"] = pd.to_numeric(df["material-assign-diversity"])
    df["active_bins"] = pd.to_numeric(df["active_material_bin_count"]).astype(int)
    df["source_materials"] = pd.to_numeric(df["source_material_count"]).astype(int)
    df["run_index"] = pd.to_numeric(df["runner_run_index"]).astype(int)
    # Variant pairs are adjacent; use first appearance to preserve sweep design order.
    order = (
        df.groupby(["scene", "max_open", "diversity"], as_index=False)["run_index"]
        .min().rename(columns={"run_index": "condition_order"})
    )
    df = df.merge(order, on=["scene", "max_open", "diversity"], how="left")
    df["condition_key"] = df.apply(
        lambda r: f"M{int(r.max_open)}_D{r.diversity:g}", axis=1
    )
    df["condition_label"] = df.apply(
        lambda r: f"{int(r.active_bins)} bins\nM{int(r.max_open)}, D{r.diversity:g}", axis=1
    )
    return df


def locate_result_csv(root: Path, run_index: int) -> Path:
    exact = list(root.glob(f"*_runs/run_{run_index:05d}.csv_{run_index}_result.csv"))
    if exact:
        return exact[0]
    matches = list(root.rglob(f"run_{run_index:05d}.csv_*_result.csv"))
    if len(matches) != 1:
        raise FileNotFoundError(f"Could not uniquely locate result CSV for run {run_index}")
    return matches[0]


def load_samples(root: Path, runs: pd.DataFrame) -> pd.DataFrame:
    parts: list[pd.DataFrame] = []
    for row in runs.itertuples(index=False):
        path = locate_result_csv(root, int(row.run_index))
        sample = pd.read_csv(path)
        if "frame" not in sample.columns or "total" not in sample.columns:
            raise KeyError(f"{path.name}: expected frame and total columns")
        sample = sample.copy()
        sample["scene"] = row.scene
        sample["renderer_variant"] = int(row.renderer_variant)
        sample["renderer"] = row.renderer
        sample["max_open"] = int(row.max_open)
        sample["diversity"] = float(row.diversity)
        sample["active_bins"] = int(row.active_bins)
        sample["source_materials"] = int(row.source_materials)
        sample["condition_order"] = int(row.condition_order)
        sample["condition_key"] = row.condition_key
        sample["condition_label"] = row.condition_label
        sample["run_index"] = int(row.run_index)
        parts.append(sample)
    return pd.concat(parts, ignore_index=True, sort=False)


def summarize_samples(samples: pd.DataFrame) -> pd.DataFrame:
    keys = ["scene", "renderer_variant", "renderer", "max_open", "diversity",
            "active_bins", "source_materials", "condition_order", "condition_key", "condition_label"]
    rows = []
    for key, g in samples.groupby(keys, sort=False, dropna=False):
        values = g["total"].astype(float)
        d = dict(zip(keys, key))
        d.update({
            "samples": len(values),
            "total_mean_ms": values.mean(),
            "total_median_ms": values.median(),
            "total_p10_ms": values.quantile(0.10),
            "total_p90_ms": values.quantile(0.90),
            "total_min_ms": values.min(),
            "total_max_ms": values.max(),
        })
        rows.append(d)
    return pd.DataFrame(rows).sort_values(["scene", "condition_order", "renderer_variant"])


def make_paired(samples: pd.DataFrame) -> tuple[pd.DataFrame, pd.DataFrame]:
    key = ["scene", "max_open", "diversity", "active_bins", "source_materials",
           "condition_order", "condition_key", "condition_label", "frame"]
    pivot = samples.pivot_table(index=key, columns="renderer_variant", values="total", aggfunc="first").reset_index()
    if 8 not in pivot.columns or 9 not in pivot.columns:
        raise ValueError("Both renderer variants 8 and 9 are required")
    pivot = pivot.rename(columns={8: "deferred_ms", 9: "visbuf_ms"})
    pivot["vis_over_deferred"] = pivot["visbuf_ms"] / pivot["deferred_ms"]
    pivot["vis_minus_deferred_ms"] = pivot["visbuf_ms"] - pivot["deferred_ms"]
    pivot["vis_overhead_percent"] = (pivot["vis_over_deferred"] - 1.0) * 100.0
    rows = []
    group_keys = key[:-1]
    for group_key, g in pivot.groupby(group_keys, sort=False):
        d = dict(zip(group_keys, group_key))
        for column in ["vis_over_deferred", "vis_minus_deferred_ms", "vis_overhead_percent"]:
            v = g[column]
            d[f"{column}_mean"] = v.mean()
            d[f"{column}_median"] = v.median()
            d[f"{column}_p10"] = v.quantile(0.10)
            d[f"{column}_p90"] = v.quantile(0.90)
        d["paired_samples"] = len(g)
        rows.append(d)
    summary = pd.DataFrame(rows).sort_values(["scene", "condition_order"])
    return pivot, summary


def style_axis(ax: plt.Axes) -> None:
    ax.grid(True, axis="y", alpha=0.28)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)


def condition_table(summary: pd.DataFrame, scene: str) -> pd.DataFrame:
    return (summary[summary.scene.eq(scene)]
            .sort_values("condition_order")
            .drop_duplicates(["condition_key"]))


def save(fig: plt.Figure, output_dir: Path, stem: str) -> None:
    fig.tight_layout()
    fig.savefig(output_dir / f"{stem}.png", dpi=220, bbox_inches="tight")
    fig.savefig(output_dir / f"{stem}.svg", bbox_inches="tight")
    plt.close(fig)


def plot_key_overview(summary: pd.DataFrame, paired_summary: pd.DataFrame, output_dir: Path) -> None:
    fig, axes = plt.subplots(2, 2, figsize=(15, 10), sharex="row")
    for row, scene in enumerate(SCENE_ORDER):
        scene_sum = summary[summary.scene.eq(scene)]
        cond = condition_table(summary, scene)
        positions = np.arange(len(cond))
        labels = cond["condition_label"].tolist()
        for renderer in RENDERER_LABELS.values():
            g = scene_sum[scene_sum.renderer.eq(renderer)].sort_values("condition_order")
            y = g["total_mean_ms"].to_numpy()
            lo = y - g["total_p10_ms"].to_numpy()
            hi = g["total_p90_ms"].to_numpy() - y
            axes[row, 0].errorbar(positions, y, yerr=[lo, hi], marker="o", capsize=3, linewidth=2, label=renderer)
        axes[row, 0].set_title(f"{scene}: total GPU time")
        axes[row, 0].set_ylabel("GPU time (ms)")
        style_axis(axes[row, 0])
        p = paired_summary[paired_summary.scene.eq(scene)].sort_values("condition_order")
        y = p["vis_over_deferred_median"].to_numpy()
        lo = y - p["vis_over_deferred_p10"].to_numpy()
        hi = p["vis_over_deferred_p90"].to_numpy() - y
        axes[row, 1].errorbar(positions, y, yerr=[lo, hi], marker="o", capsize=3, linewidth=2)
        axes[row, 1].axhline(1.0, color="black", linewidth=1, linestyle="--")
        axes[row, 1].set_title(f"{scene}: paired visibility/deferred ratio")
        axes[row, 1].set_ylabel("Visibility / deferred")
        style_axis(axes[row, 1])
        for col in range(2):
            axes[row, col].set_xticks(positions, labels, rotation=30, ha="right")
    axes[0, 0].legend(frameon=False)
    fig.suptitle("Material-bin sweep overview\nError bars show camera-path P10–P90", fontsize=15)
    save(fig, output_dir, "00_sweep_overview")


def plot_absolute(summary: pd.DataFrame, output_dir: Path) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(15, 5.8))
    for ax, scene in zip(axes, SCENE_ORDER):
        cond = condition_table(summary, scene)
        positions = np.arange(len(cond))
        for renderer in RENDERER_LABELS.values():
            g = summary[(summary.scene == scene) & (summary.renderer == renderer)].sort_values("condition_order")
            ax.plot(positions, g["total_mean_ms"], marker="o", linewidth=2.2, label=renderer)
            ax.fill_between(positions, g["total_p10_ms"], g["total_p90_ms"], alpha=0.14)
        ax.set_xticks(positions, cond["condition_label"], rotation=30, ha="right")
        ax.set_title(scene)
        ax.set_ylabel("Total GPU time (ms)")
        style_axis(ax)
    axes[0].legend(frameon=False)
    fig.suptitle("Total GPU time across the bin sweep\nLines are means; bands are camera-path P10–P90")
    save(fig, output_dir, "01_total_time_sweep")


def plot_ratio(paired_summary: pd.DataFrame, output_dir: Path) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(15, 5.8))
    for ax, scene in zip(axes, SCENE_ORDER):
        g = paired_summary[paired_summary.scene.eq(scene)].sort_values("condition_order")
        positions = np.arange(len(g))
        med = g["vis_over_deferred_median"].to_numpy()
        ax.plot(positions, med, marker="o", linewidth=2.4)
        ax.fill_between(positions, g["vis_over_deferred_p10"], g["vis_over_deferred_p90"], alpha=0.16)
        ax.axhline(1.0, color="black", linewidth=1, linestyle="--")
        ax.set_xticks(positions, g["condition_label"], rotation=30, ha="right")
        ax.set_title(scene)
        ax.set_ylabel("Visibility / deferred time")
        style_axis(ax)
        for x, value in zip(positions, med):
            ax.annotate(f"{value:.2f}×", (x, value), xytext=(0, 7), textcoords="offset points", ha="center", fontsize=8)
    fig.suptitle("Paired renderer cost ratio across the sweep\nBand is the camera-path P10–P90 ratio")
    save(fig, output_dir, "02_paired_renderer_ratio")


def plot_normalized(summary: pd.DataFrame, output_dir: Path) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(15, 5.8))
    for ax, scene in zip(axes, SCENE_ORDER):
        cond = condition_table(summary, scene)
        positions = np.arange(len(cond))
        for renderer in RENDERER_LABELS.values():
            g = summary[(summary.scene == scene) & (summary.renderer == renderer)].sort_values("condition_order").copy()
            baseline = float(g.iloc[0]["total_mean_ms"])
            g["normalized"] = g["total_mean_ms"] / baseline
            ax.plot(positions, g["normalized"], marker="o", linewidth=2.2, label=renderer)
        ax.axhline(1.0, color="black", linewidth=1, linestyle="--")
        ax.set_xticks(positions, cond["condition_label"], rotation=30, ha="right")
        ax.set_title(scene)
        ax.set_ylabel("Time / 1-bin baseline")
        style_axis(ax)
    axes[0].legend(frameon=False)
    fig.suptitle("Sweep sensitivity normalized to the 1-bin condition")
    save(fig, output_dir, "03_normalized_to_one_bin")


def plot_vis_pass_breakdown(samples: pd.DataFrame, output_dir: Path) -> pd.DataFrame:
    vis = samples[samples.renderer_variant.eq(9)].copy()
    rows = []
    keys = ["scene", "max_open", "diversity", "active_bins", "condition_order", "condition_key", "condition_label"]
    for key, g in vis.groupby(keys, sort=False):
        d = dict(zip(keys, key))
        for group_name, columns in PASS_GROUPS_VIS.items():
            present = [c for c in columns if c in g.columns]
            d[group_name] = g[present].fillna(0).sum(axis=1).mean() if present else 0.0
        d["Total"] = g["total"].mean()
        d["Other"] = max(0.0, d["Total"] - sum(d[name] for name in PASS_GROUPS_VIS))
        rows.append(d)
    table = pd.DataFrame(rows).sort_values(["scene", "condition_order"])
    fig, axes = plt.subplots(1, 2, figsize=(16, 6.2))
    stack_names = [*PASS_GROUPS_VIS.keys(), "Other"]
    for ax, scene in zip(axes, SCENE_ORDER):
        g = table[table.scene.eq(scene)].sort_values("condition_order")
        positions = np.arange(len(g))
        bottom = np.zeros(len(g))
        for name in stack_names:
            values = g[name].to_numpy()
            ax.bar(positions, values, bottom=bottom, label=name)
            bottom += values
        ax.plot(positions, g["Total"], color="black", marker="o", linewidth=1.6, label="Measured total")
        ax.set_xticks(positions, g["condition_label"], rotation=30, ha="right")
        ax.set_title(scene)
        ax.set_ylabel("Mean GPU time (ms)")
        style_axis(ax)
    axes[0].legend(frameon=False, ncol=2)
    fig.suptitle("Visibility + G-buffer pass composition across the bin sweep")
    save(fig, output_dir, "04_visibility_pass_breakdown")
    return table


def plot_heatmap(paired: pd.DataFrame, output_dir: Path) -> None:
    fig, axes = plt.subplots(2, 1, figsize=(15, 10), constrained_layout=True)
    all_values = paired["vis_over_deferred"].replace([np.inf, -np.inf], np.nan).dropna()
    vmin = max(0.5, float(all_values.quantile(0.01)))
    vmax = max(1.01, float(all_values.quantile(0.99)))
    norm = TwoSlopeNorm(vmin=vmin, vcenter=1.0, vmax=vmax)
    image = None
    for ax, scene in zip(axes, SCENE_ORDER):
        g = paired[paired.scene.eq(scene)].sort_values(["frame", "condition_order"])
        matrix = g.pivot(index="frame", columns="condition_order", values="vis_over_deferred")
        label_map = g.drop_duplicates("condition_order").set_index("condition_order")["condition_label"]
        matrix = matrix.reindex(columns=sorted(matrix.columns))
        image = ax.imshow(matrix.to_numpy(), aspect="auto", origin="lower", cmap="coolwarm", norm=norm)
        ax.set_xticks(np.arange(len(matrix.columns)), [label_map[c] for c in matrix.columns], rotation=30, ha="right")
        y_ticks = np.linspace(0, len(matrix.index)-1, min(7, len(matrix.index))).round().astype(int)
        ax.set_yticks(y_ticks, [str(matrix.index[i]) for i in y_ticks])
        ax.set_title(scene)
        ax.set_ylabel("Camera-path frame")
    axes[-1].set_xlabel("Sweep condition")
    if image is not None:
        fig.colorbar(image, ax=axes.ravel().tolist(), label="Visibility / deferred", shrink=0.88, pad=0.02)
    fig.suptitle("Paired renderer ratio over camera path and sweep condition")
    fig.savefig(output_dir / "05_camera_path_ratio_heatmap.png", dpi=220, bbox_inches="tight")
    fig.savefig(output_dir / "05_camera_path_ratio_heatmap.svg", bbox_inches="tight")
    plt.close(fig)


def diversity_contrasts(summary: pd.DataFrame) -> pd.DataFrame:
    subset = summary[summary["diversity"].isin([0.0, 1.0]) & summary["max_open"].isin([64, 255])].copy()
    keys = ["scene", "renderer", "max_open", "active_bins"]
    p = subset.pivot_table(index=keys, columns="diversity", values="total_mean_ms", aggfunc="first").reset_index()
    p = p.rename(columns={0.0: "diversity_0_ms", 1.0: "diversity_1_ms"})
    p["diversity_1_vs_0_percent"] = (p["diversity_1_ms"] / p["diversity_0_ms"] - 1) * 100
    return p.sort_values(["scene", "renderer", "max_open"])


def plot_diversity_contrasts(contrast: pd.DataFrame, output_dir: Path) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(14, 5.8), sharey=True)
    for ax, scene in zip(axes, SCENE_ORDER):
        g = contrast[contrast.scene.eq(scene)].copy()
        labels = [f"{renderer}\nM{max_open} / {active_bins} bins" for renderer, max_open, active_bins in zip(g.renderer, g.max_open, g.active_bins)]
        values = g["diversity_1_vs_0_percent"].to_numpy()
        positions = np.arange(len(g))
        ax.bar(positions, values)
        ax.axhline(0, color="black", linewidth=1)
        ax.set_xticks(positions, labels, rotation=25, ha="right")
        ax.set_title(scene)
        ax.set_ylabel("Diversity 1 vs 0 (%)")
        style_axis(ax)
        for x, value in zip(positions, values):
            va = "bottom" if value >= 0 else "top"
            offset = 3 if value >= 0 else -3
            ax.annotate(f"{value:+.1f}%", (x, value), xytext=(0, offset), textcoords="offset points", ha="center", va=va, fontsize=9)
    fig.suptitle("Matched diversity contrast at equal active-bin counts\nNegative values mean diversity=1 is faster")
    save(fig, output_dir, "06_matched_diversity_contrast")


def plot_bin_realization(runs: pd.DataFrame, output_dir: Path) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(14, 5.6))
    unique = runs.drop_duplicates(["scene", "max_open", "diversity", "active_bins"])
    marker_map = {0.0: "s", 0.5: "^", 1.0: "o"}
    for ax, scene in zip(axes, SCENE_ORDER):
        g = unique[unique.scene.eq(scene)].sort_values("condition_order")
        for diversity, dg in g.groupby("diversity", sort=True):
            ax.scatter(dg["max_open"], dg["active_bins"], marker=marker_map.get(float(diversity), "o"), s=70, label=f"Diversity {diversity:g}")
        source = int(g["source_materials"].iloc[0])
        ax.axhline(source, color="black", linewidth=1, linestyle="--", label=f"Source materials ({source})")
        ax.set_xscale("log", base=2)
        ax.set_xticks(sorted(g["max_open"].unique()), [str(x) for x in sorted(g["max_open"].unique())])
        ax.set_xlabel("Requested max-open")
        ax.set_ylabel("Actual active material bins")
        ax.set_title(scene)
        style_axis(ax)
    axes[0].legend(frameon=False)
    fig.suptitle("Requested bin cap versus realized active bins")
    save(fig, output_dir, "07_bin_cap_realization")


def write_readme(output_dir: Path, runs: pd.DataFrame, summary: pd.DataFrame, paired_summary: pd.DataFrame, contrast: pd.DataFrame) -> None:
    lines = [
        "# Real Sponza/Bistro material-bin sweep plots",
        "",
        f"- Successful runs: {len(runs)}",
        f"- Camera-path samples per run: {int(summary['samples'].mode().iloc[0])}",
        "- Renderers: Deferred + prepass (variant 8), Visibility + G-buffer (variant 9)",
        "- Main x-axis: realized active material-bin count, with requested max-open and diversity in labels.",
        "",
        "## Figures",
        "",
        "- `00_sweep_overview`: absolute total time and paired renderer ratio.",
        "- `01_total_time_sweep`: mean total time with camera-path P10–P90 bands.",
        "- `02_paired_renderer_ratio`: frame-matched visibility/deferred ratio.",
        "- `03_normalized_to_one_bin`: renderer sensitivity relative to the 1-bin baseline.",
        "- `04_visibility_pass_breakdown`: visibility renderer pass composition.",
        "- `05_camera_path_ratio_heatmap`: ratio across camera path and sweep conditions.",
        "- `06_matched_diversity_contrast`: diversity 1 versus 0 at equal active-bin counts.",
        "- `07_bin_cap_realization`: requested cap versus actual bins.",
        "",
        "## Notes",
        "",
        "P10–P90 ranges describe variation along the camera path, not repeated-run confidence intervals. Each condition was run once.",
    ]
    (output_dir / "README.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    root, temp = prepare_input(args.input)
    try:
        main_csv = find_main_csv(root)
        runs = validate_and_prepare(main_csv)
        samples = load_samples(root, runs)
        summary = summarize_samples(samples)
        paired, paired_summary = make_paired(samples)
        contrast = diversity_contrasts(summary)

        runs.to_csv(args.output_dir / "runs_prepared.csv", index=False)
        samples.to_csv(args.output_dir / "camera_path_samples.csv", index=False)
        summary.to_csv(args.output_dir / "sweep_summary.csv", index=False)
        paired.to_csv(args.output_dir / "paired_camera_path_metrics.csv", index=False)
        paired_summary.to_csv(args.output_dir / "paired_sweep_summary.csv", index=False)
        contrast.to_csv(args.output_dir / "matched_diversity_contrasts.csv", index=False)

        plot_key_overview(summary, paired_summary, args.output_dir)
        plot_absolute(summary, args.output_dir)
        plot_ratio(paired_summary, args.output_dir)
        plot_normalized(summary, args.output_dir)
        pass_table = plot_vis_pass_breakdown(samples, args.output_dir)
        pass_table.to_csv(args.output_dir / "visibility_pass_breakdown.csv", index=False)
        plot_heatmap(paired, args.output_dir)
        plot_diversity_contrasts(contrast, args.output_dir)
        plot_bin_realization(runs, args.output_dir)
        write_readme(args.output_dir, runs, summary, paired_summary, contrast)

        manifest = {
            "input": str(args.input),
            "main_csv": main_csv.name,
            "successful_runs": int(len(runs)),
            "camera_path_samples": int(len(samples)),
            "scenes": sorted(runs["scene"].unique().tolist()),
            "renderers": sorted(runs["renderer"].unique().tolist()),
            "figures": sorted(p.name for p in args.output_dir.glob("*.png")),
        }
        (args.output_dir / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
        print(summary[["scene", "renderer", "condition_label", "total_mean_ms"]].to_string(index=False))
        print(f"\nWrote plots to {args.output_dir.resolve()}")
    finally:
        if temp is not None:
            temp.cleanup()


if __name__ == "__main__":
    main()
