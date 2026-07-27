#!/usr/bin/env python3
"""Broad Sponza analysis: summaries, ALU trends, winner maps, pass and geometry plots."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from common import PASS_COLUMNS, VARIANT_ORDER, ensure_output_dir, load_experiment_csv, parse_int_list


def save(path: Path) -> None:
    plt.tight_layout()
    plt.savefig(path, dpi=180, bbox_inches="tight")
    plt.close()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_csv", type=Path)
    parser.add_argument("--alu-values", default="10,20,40,80,160")
    parser.add_argument("--output-dir", type=Path, default=Path("visibility_buffer_analysis"))
    args = parser.parse_args()

    alus = parse_int_list(args.alu_values)
    out = ensure_output_dir(args.output_dir.resolve(), clean=True)
    plots = ensure_output_dir(out / "plots")
    df = load_experiment_csv(args.input_csv.resolve(), alus)
    df["pass_sum"] = df[PASS_COLUMNS].sum(axis=1, skipna=True)
    df["unattributed"] = df["total"] - df["pass_sum"]

    summary = df.groupby(["renderer_variant", "variant", "alu_calc_count"], as_index=False).agg(
        windows=("frame", "size"),
        mean_ms=("total", "mean"),
        median_ms=("total", "median"),
        p95_ms=("total", lambda s: s.quantile(0.95)),
        p99_ms=("total", lambda s: s.quantile(0.99)),
        min_ms=("total", "min"),
        max_ms=("total", "max"),
        std_ms=("total", "std"),
        mean_index_count=("index_count", "mean"),
        median_index_count=("index_count", "median"),
    ).sort_values(["renderer_variant", "alu_calc_count"])
    summary["cv_percent"] = summary["std_ms"] / summary["mean_ms"] * 100
    summary.to_csv(out / "run_summary.csv", index=False, encoding="utf-8-sig")

    pass_summary = df.groupby(["renderer_variant", "variant", "alu_calc_count"], as_index=False)[PASS_COLUMNS + ["total", "pass_sum", "unattributed"]].median().sort_values(["renderer_variant", "alu_calc_count"])
    pass_summary.to_csv(out / "pass_median_summary.csv", index=False, encoding="utf-8-sig")

    fit_rows = []
    for variant in VARIANT_ORDER:
        group = summary[summary["variant"] == variant].sort_values("alu_calc_count")
        x = group["alu_calc_count"].to_numpy(float)
        y = group["median_ms"].to_numpy(float)
        slope, intercept = np.polyfit(x, y, 1)
        pred = intercept + slope * x
        ss_res = np.sum((y - pred) ** 2)
        ss_tot = np.sum((y - y.mean()) ** 2)
        fit_rows.append({"variant": variant, "intercept_ms": intercept, "slope_ms_per_alu": slope, "r2": 1 - ss_res / ss_tot})
    fit_df = pd.DataFrame(fit_rows)
    fit_df.to_csv(out / "alu_linear_fit.csv", index=False, encoding="utf-8-sig")

    median_pivot = summary.pivot(index="alu_calc_count", columns="variant", values="median_ms").reindex(columns=VARIANT_ORDER)
    speedup = pd.DataFrame(index=median_pivot.index)
    for variant in VARIANT_ORDER:
        speedup[f"Forward_over_{variant}"] = median_pivot["Forward"] / median_pivot[variant]
    speedup.to_csv(out / "median_speedup.csv", encoding="utf-8-sig")

    frame_pivot = df.pivot_table(index=["alu_calc_count", "frame"], columns="variant", values="total").reindex(columns=VARIANT_ORDER)
    winner = frame_pivot.idxmin(axis=1).rename("winner").reset_index()
    winner_counts = winner.groupby(["alu_calc_count", "winner"]).size().unstack(fill_value=0).reindex(index=alus, columns=VARIANT_ORDER, fill_value=0)
    winner_share = winner_counts.div(winner_counts.sum(axis=1), axis=0) * 100
    winner_share.to_csv(out / "frame_winner_share_percent.csv", encoding="utf-8-sig")

    for alu in alus:
        plt.figure(figsize=(12, 6))
        for variant in VARIANT_ORDER:
            group = df[(df["alu_calc_count"] == alu) & (df["variant"] == variant)]
            plt.plot(group["frame"], group["total"], label=variant)
        plt.title(f"Total GPU Time by Frame — ALU {alu}")
        plt.xlabel("Frame")
        plt.ylabel("GPU time (ms)")
        plt.grid(True, alpha=0.3)
        plt.legend(ncol=2)
        save(plots / f"frame_time_alu_{alu}.png")

    for metric, title, filename in [("median_ms", "Median GPU Time vs ALU Workload", "median_time_vs_alu.png"), ("p95_ms", "P95 GPU Time vs ALU Workload", "p95_time_vs_alu.png")]:
        plt.figure(figsize=(10, 6))
        for variant in VARIANT_ORDER:
            group = summary[summary["variant"] == variant].sort_values("alu_calc_count")
            plt.plot(group["alu_calc_count"], group[metric], marker="o", label=variant)
        plt.title(title)
        plt.xlabel("ALU calculation count")
        plt.ylabel("GPU time (ms)")
        plt.grid(True, alpha=0.3)
        plt.legend(ncol=2)
        save(plots / filename)

    plt.figure(figsize=(10, 6))
    for variant in VARIANT_ORDER:
        values = median_pivot["Forward"] / median_pivot[variant]
        plt.plot(values.index, values.values, marker="o", label=variant)
    plt.axhline(1.0, linewidth=1)
    plt.title("Median Speedup Relative to Forward")
    plt.xlabel("ALU calculation count")
    plt.ylabel("Speedup")
    plt.grid(True, alpha=0.3)
    plt.legend(ncol=2)
    save(plots / "speedup_vs_forward.png")

    plt.figure(figsize=(10, 6))
    bottom = np.zeros(len(winner_share))
    for variant in VARIANT_ORDER:
        values = winner_share[variant].to_numpy()
        plt.bar(winner_share.index.astype(str), values, bottom=bottom, label=variant)
        bottom += values
    plt.title("Fastest Variant Share Across Frames")
    plt.xlabel("ALU calculation count")
    plt.ylabel("Winning frames (%)")
    plt.legend(ncol=2)
    save(plots / "winner_share.png")

    max_alu = max(alus)
    breakdown = pass_summary[pass_summary["alu_calc_count"] == max_alu].set_index("variant").reindex(VARIANT_ORDER).reset_index()
    plt.figure(figsize=(12, 7))
    bottom = np.zeros(len(breakdown))
    for column in PASS_COLUMNS + ["unattributed"]:
        values = breakdown[column].fillna(0).to_numpy()
        if np.allclose(values, 0):
            continue
        plt.bar(breakdown["variant"], values, bottom=bottom, label=column)
        bottom += values
    plt.title(f"Median Pass Breakdown — ALU {max_alu}")
    plt.xlabel("Renderer variant")
    plt.ylabel("Median GPU time (ms)")
    plt.xticks(rotation=25, ha="right")
    plt.legend(ncol=3)
    save(plots / f"pass_breakdown_alu_{max_alu}.png")

    index_by_frame = df.groupby("frame", as_index=False)["index_count"].mean()
    plt.figure(figsize=(12, 5))
    plt.plot(index_by_frame["frame"], index_by_frame["index_count"])
    plt.title("View-Frustum-Culled Index Count by Frame")
    plt.xlabel("Frame")
    plt.ylabel("Index count")
    plt.grid(True, alpha=0.3)
    save(plots / "index_count_vs_frame.png")

    correlations = df.groupby(["variant", "alu_calc_count"]).apply(lambda g: g["total"].corr(g["index_count"]), include_groups=False).unstack().reindex(index=VARIANT_ORDER, columns=alus)
    correlations.to_csv(out / "total_time_index_correlation.csv", encoding="utf-8-sig")

    tvb = df[df["variant"] == "TVB"].groupby("frame", as_index=False).agg(visibility_ms=("visibility", "mean"), index_count=("index_count", "mean"))
    slope, intercept = np.polyfit(tvb["index_count"], tvb["visibility_ms"], 1)
    tvb["predicted_ms"] = intercept + slope * tvb["index_count"]
    tvb["residual_ms"] = tvb["visibility_ms"] - tvb["predicted_ms"]
    tvb.sort_values("residual_ms", ascending=False).to_csv(out / "visibility_residual_by_frame.csv", index=False, encoding="utf-8-sig")
    plt.figure(figsize=(12, 6))
    plt.plot(tvb["frame"], tvb["residual_ms"])
    plt.axhline(0.0, linewidth=1)
    plt.title("TVB Visibility Residual After Index-Count Regression")
    plt.xlabel("Frame")
    plt.ylabel("Residual visibility time (ms)")
    plt.grid(True, alpha=0.3)
    save(plots / "visibility_residual_vs_frame.png")

    (out / "key_findings.json").write_text(json.dumps({"rows": int(len(df)), "runs": int(df["run_id"].nunique()), "windows_per_run": int(df.groupby("run_id").size().median())}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"[완료] {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
