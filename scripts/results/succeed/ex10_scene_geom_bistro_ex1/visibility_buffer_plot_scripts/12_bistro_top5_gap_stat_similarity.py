#!/usr/bin/env python3
from __future__ import annotations

import argparse
import zipfile
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from common import align_raster_stats, ensure_output_dir, load_experiment_csv

GAP_SPECS = {
    "Forward - Forward+Prepass": ("Forward", "Forward+Prepass"),
    "Forward+Prepass - TVB": ("Forward+Prepass", "TVB"),
}


def zscore(series: pd.Series) -> pd.Series:
    std = series.std(ddof=0)
    if std == 0 or np.isnan(std):
        return pd.Series(np.zeros(len(series)), index=series.index)
    return (series - series.mean()) / std


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("result_csv", type=Path)
    parser.add_argument("raster_stats_csv", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("bistro_top5_gap_stat_similarity"))
    parser.add_argument("--zip-output", type=Path, default=Path("bistro_top5_gap_stat_similarity.zip"))
    parser.add_argument("--window-frames", type=int, default=10)
    parser.add_argument("--rank-absolute", action="store_true")
    args = parser.parse_args()

    out = ensure_output_dir(args.output_dir.resolve(), clean=True)
    results = load_experiment_csv(args.result_csv.resolve(), [1, 5, 10, 20])
    raw_stats = pd.read_csv(args.raster_stats_csv.resolve())
    stats = align_raster_stats(raw_stats, args.window_frames)
    stat_columns = [column for column in raw_stats.columns if column != "frame"]
    target_alus = [5, 10, 20]
    aligned_by_alu = {}

    for alu in target_alus:
        gpu = results[(results["alu_calc_count"] == alu) & results["variant"].isin(["Forward", "Forward+Prepass", "TVB"])][["frame", "variant", "total"]]
        wide = gpu.pivot(index="frame", columns="variant", values="total").reset_index()
        aligned = wide.merge(stats, on="frame", how="inner")
        for gap_name, (left, right) in GAP_SPECS.items():
            aligned[gap_name] = aligned[left] - aligned[right]
        aligned_by_alu[alu] = aligned

    rows = []
    for gap_name in GAP_SPECS:
        for stat_name in stat_columns:
            row = {"gap_series": gap_name, "stat": stat_name}
            correlations = []
            for alu in target_alus:
                corr = aligned_by_alu[alu][gap_name].corr(aligned_by_alu[alu][stat_name])
                row[f"pearson_alu_{alu}"] = corr
                correlations.append(corr)
            row["mean_pearson"] = float(np.mean(correlations))
            row["mean_abs_pearson"] = float(np.mean(np.abs(correlations)))
            row["min_pearson"] = float(np.min(correlations))
            row["max_pearson"] = float(np.max(correlations))
            rows.append(row)

    ranking = pd.DataFrame(rows)
    key = "mean_abs_pearson" if args.rank_absolute else "mean_pearson"
    ranking = ranking.sort_values(key, ascending=False).reset_index(drop=True)
    ranking.insert(0, "rank", np.arange(1, len(ranking) + 1))
    ranking.to_csv(out / "gap_stat_similarity_ranking.csv", index=False, encoding="utf-8-sig")
    top5 = ranking.head(5).copy()
    top5.to_csv(out / "top5_gap_stat_pairs.csv", index=False, encoding="utf-8-sig")

    created = []
    for pair in top5.itertuples(index=False):
        fig, axes = plt.subplots(3, 1, figsize=(15, 11), sharex=True, sharey=True)
        for axis, alu in zip(axes, target_alus):
            aligned = aligned_by_alu[alu]
            corr = getattr(pair, f"pearson_alu_{alu}")
            axis.plot(aligned["frame"], zscore(aligned[pair.gap_series]), label=pair.gap_series)
            axis.plot(aligned["frame"], zscore(aligned[pair.stat]), linestyle="--", label=pair.stat)
            axis.set_title(f"ALU {alu} — Pearson r = {corr:.3f}")
            axis.set_ylabel("z-score")
            axis.grid(True, alpha=0.3)
            axis.legend(ncol=2)
        axes[-1].set_xlabel("Profile frame")
        score = getattr(pair, key)
        fig.suptitle(f"Rank {pair.rank}: {pair.gap_series} vs {pair.stat}\n{key} = {score:.3f}")
        fig.tight_layout(rect=[0, 0, 1, 0.96])
        safe_gap = pair.gap_series.lower().replace(" ", "_").replace("+", "plus").replace("-", "minus")
        path = out / f"rank_{pair.rank:02d}_{safe_gap}_vs_{pair.stat}.png"
        fig.savefig(path, dpi=180, bbox_inches="tight")
        plt.close(fig)
        created.append(path)

    if args.zip_output.exists():
        args.zip_output.unlink()
    with zipfile.ZipFile(args.zip_output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in created:
            archive.write(path, arcname=path.name)
        archive.write(out / "top5_gap_stat_pairs.csv", arcname="top5_gap_stat_pairs.csv")
        archive.write(out / "gap_stat_similarity_ranking.csv", arcname="gap_stat_similarity_ranking.csv")
    print(top5[["rank", "gap_series", "stat", "mean_pearson", "mean_abs_pearson"]].to_string(index=False))
    print(f"[완료] {args.zip_output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
