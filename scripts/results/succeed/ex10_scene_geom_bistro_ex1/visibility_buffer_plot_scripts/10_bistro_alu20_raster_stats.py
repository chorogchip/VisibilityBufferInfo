#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd

from common import align_raster_stats, ensure_output_dir, load_experiment_csv


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("result_csv", type=Path)
    parser.add_argument("raster_stats_csv", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("bistro_stats_alu20"))
    parser.add_argument("--window-frames", type=int, default=10)
    args = parser.parse_args()

    out = ensure_output_dir(args.output_dir.resolve(), clean=True)
    results = load_experiment_csv(args.result_csv.resolve(), [1, 5, 10, 20])
    stats = align_raster_stats(pd.read_csv(args.raster_stats_csv.resolve()), args.window_frames)
    stat_columns = ["total_fragments", "covered_pixels", "avg_overdraw", "rasterized_triangles", "quad_efficiency"]
    variants = ["Forward", "Forward+Prepass", "TVB"]

    gpu = results[(results["alu_calc_count"] == 20) & (results["variant"].isin(variants))][["frame", "variant", "total"]]
    wide = gpu.pivot(index="frame", columns="variant", values="total").reset_index()
    combined = wide.merge(stats[["frame"] + stat_columns], on="frame", how="inner")
    combined.to_csv(out / "bistro_alu20_gpu_and_raster_stats.csv", index=False, encoding="utf-8-sig")

    rows = []
    for variant in variants:
        for stat in stat_columns:
            rows.append({"variant": variant, "stat": stat, "pearson_correlation": combined[variant].corr(combined[stat])})
    pd.DataFrame(rows).to_csv(out / "bistro_alu20_stat_correlations.csv", index=False, encoding="utf-8-sig")

    fig, axes = plt.subplots(3, 1, figsize=(15, 11), sharex=True, gridspec_kw={"height_ratios": [1.2, 1.0, 1.0]})
    for variant in variants:
        axes[0].plot(combined["frame"], combined[variant], label=variant)
    axes[0].set_title("Bistro Exterior — ALU 20 GPU Time and Raster Statistics")
    axes[0].set_ylabel("Total GPU time (ms)")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(ncol=3)

    axes[1].plot(combined["frame"], combined["avg_overdraw"], label="Average overdraw")
    axes[1].set_ylabel("Average overdraw")
    axes[1].grid(True, alpha=0.3)
    second = axes[1].twinx()
    second.plot(combined["frame"], combined["quad_efficiency"] * 100, linestyle="--", label="Quad efficiency")
    second.set_ylabel("Quad efficiency (%)")
    h1, l1 = axes[1].get_legend_handles_labels()
    h2, l2 = second.get_legend_handles_labels()
    axes[1].legend(h1 + h2, l1 + l2, ncol=2)

    for column, label in [("rasterized_triangles", "Rasterized triangles"), ("total_fragments", "Total fragments"), ("covered_pixels", "Covered pixels")]:
        axes[2].plot(combined["frame"], combined[column] / 1_000_000, label=label)
    axes[2].set_xlabel("Profile frame")
    axes[2].set_ylabel("Count (millions)")
    axes[2].grid(True, alpha=0.3)
    axes[2].legend(ncol=3)

    fig.tight_layout()
    fig.savefig(out / "bistro_alu20_gpu_time_with_raster_stats.png", dpi=180, bbox_inches="tight")
    plt.close(fig)
    print(f"[완료] {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
