#!/usr/bin/env python3
"""
VisibilityBufferInfo camera-path timing plotter.

Usage:
    python plot_results.py --input-dir . --output-dir plot_results

Expected input files:
    temp_forward.csv
    temp_forward_prepass.csv
    temp_deferred.csv
    temp_deferred_prepass.csv
    temp_visbuf.csv
    temp_visbuf_gbuffer.csv
"""

from pathlib import Path
import argparse
import json
import zipfile
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", type=Path, default=Path("."))
    parser.add_argument("--output-dir", type=Path, default=Path("plot_results"))
    parser.add_argument(
        "--unit",
        default="ms",
        help="Unit label used on plots. The CSV files themselves do not contain unit metadata.",
    )
    return parser.parse_args()


def save_current(output_dir: Path, filename: str) -> None:
    plt.tight_layout()
    plt.savefig(output_dir / filename, dpi=180, bbox_inches="tight")
    plt.close()


def main():
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    input_files = {
        "Forward": args.input_dir / "temp_forward.csv",
        "Forward + Prepass": args.input_dir / "temp_forward_prepass.csv",
        "Deferred": args.input_dir / "temp_deferred.csv",
        "Deferred + Prepass": args.input_dir / "temp_deferred_prepass.csv",
        "TVB": args.input_dir / "temp_visbuf.csv",
        "TVB + G-buffer": args.input_dir / "temp_visbuf_gbuffer.csv",
    }

    missing = [str(path) for path in input_files.values() if not path.exists()]
    if missing:
        raise FileNotFoundError("Missing input files:\n" + "\n".join(missing))

    data = {name: pd.read_csv(path) for name, path in input_files.items()}
    base_frames = next(iter(data.values()))["frame"]

    for name, df in data.items():
        if "frame" not in df.columns:
            raise ValueError(f"{name}: missing frame column")
        if not df["frame"].equals(base_frames):
            raise ValueError(f"{name}: frame indices do not align")
        if df.drop(columns="frame").isna().any().any():
            raise ValueError(f"{name}: contains missing timing values")

    renderer_columns = list(input_files.keys())
    totals = pd.DataFrame({"frame": base_frames})
    for name, df in data.items():
        totals[name] = df.drop(columns="frame").sum(axis=1)

    summary_rows = []
    for name in renderer_columns:
        s = totals[name]
        summary_rows.append({
            "renderer": name,
            "samples": len(s),
            "mean": s.mean(),
            "median": s.median(),
            "std": s.std(ddof=1),
            "cv_percent": s.std(ddof=1) / s.mean() * 100,
            "min": s.min(),
            "p05": s.quantile(0.05),
            "p95": s.quantile(0.95),
            "p99": s.quantile(0.99),
            "max": s.max(),
            "mean_vs_forward_percent": (s.mean() / totals["Forward"].mean() - 1) * 100,
        })
    summary = pd.DataFrame(summary_rows).sort_values("mean").reset_index(drop=True)

    pass_rows = []
    for name, df in data.items():
        total_mean = totals[name].mean()
        for pass_name in [column for column in df.columns if column != "frame"]:
            mean_value = df[pass_name].mean()
            pass_rows.append({
                "renderer": name,
                "pass": pass_name,
                "mean": mean_value,
                "share_percent": mean_value / total_mean * 100,
            })
    pass_summary = pd.DataFrame(pass_rows)

    winner = totals[renderer_columns].idxmin(axis=1)
    winner_time = totals[renderer_columns].min(axis=1)
    winner_table = pd.DataFrame({
        "frame": totals["frame"],
        "winner": winner,
        "winner_time": winner_time,
    })
    winner_counts = (
        winner.value_counts()
        .rename_axis("renderer")
        .reset_index(name="winning_samples")
    )
    winner_counts["winning_percent"] = winner_counts["winning_samples"] / len(totals) * 100

    deltas = pd.DataFrame({
        "frame": totals["frame"],
        "TVB - Forward": totals["TVB"] - totals["Forward"],
        "TVB - Deferred": totals["TVB"] - totals["Deferred"],
        "TVB + G-buffer - Deferred": totals["TVB + G-buffer"] - totals["Deferred"],
        "Forward + Prepass - Forward": totals["Forward + Prepass"] - totals["Forward"],
        "Deferred + Prepass - Deferred": totals["Deferred + Prepass"] - totals["Deferred"],
    })
    correlation = totals[renderer_columns].corr()

    totals.to_csv(args.output_dir / "total_time_per_frame.csv", index=False)
    summary.to_csv(args.output_dir / "summary_statistics.csv", index=False)
    pass_summary.to_csv(args.output_dir / "pass_summary.csv", index=False)
    winner_table.to_csv(args.output_dir / "winner_per_frame.csv", index=False)
    winner_counts.to_csv(args.output_dir / "winner_counts.csv", index=False)
    deltas.to_csv(args.output_dir / "pairwise_time_deltas.csv", index=False)
    correlation.to_csv(args.output_dir / "renderer_correlation.csv")

    ylabel = f"Total GPU time ({args.unit})"
    pass_ylabel = f"Pass GPU time ({args.unit})"

    plt.figure(figsize=(13, 7))
    for name in renderer_columns:
        plt.plot(totals["frame"], totals[name], label=name)
    plt.xlabel("Frame")
    plt.ylabel(ylabel)
    plt.title("Total GPU Time by Camera-Path Frame")
    plt.grid(True, alpha=0.3)
    plt.legend(ncol=2)
    save_current(args.output_dir, "01_total_time_by_frame.png")

    window = 5
    plt.figure(figsize=(13, 7))
    for name in renderer_columns:
        plt.plot(
            totals["frame"],
            totals[name].rolling(window=window, center=True, min_periods=1).mean(),
            label=name,
        )
    plt.xlabel("Frame")
    plt.ylabel(f"Rolling mean GPU time ({args.unit})")
    plt.title(f"Smoothed GPU Time by Frame ({window}-Sample Rolling Mean)")
    plt.grid(True, alpha=0.3)
    plt.legend(ncol=2)
    save_current(args.output_dir, "02_rolling_mean_by_frame.png")

    plt.figure(figsize=(13, 7))
    for name in renderer_columns:
        if name != "Forward":
            plt.plot(totals["frame"], totals[name] / totals["Forward"], label=name)
    plt.axhline(1.0)
    plt.xlabel("Frame")
    plt.ylabel("Time / Forward time")
    plt.title("Per-Frame Cost Relative to Forward")
    plt.grid(True, alpha=0.3)
    plt.legend(ncol=2)
    save_current(args.output_dir, "03_relative_to_forward.png")

    plt.figure(figsize=(12, 7))
    plt.boxplot(
        [totals[name] for name in renderer_columns],
        tick_labels=renderer_columns,
        showmeans=True,
    )
    plt.ylabel(ylabel)
    plt.title("Frame-Time Distribution by Renderer")
    plt.xticks(rotation=25, ha="right")
    plt.grid(True, axis="y", alpha=0.3)
    save_current(args.output_dir, "04_time_distribution_boxplot.png")

    ordered = summary["renderer"].tolist()
    x = np.arange(len(ordered))
    indexed_summary = summary.set_index("renderer")
    plt.figure(figsize=(12, 7))
    plt.plot(x, indexed_summary.loc[ordered, "mean"], marker="o", label="Mean")
    plt.plot(x, indexed_summary.loc[ordered, "median"], marker="o", label="Median")
    plt.plot(x, indexed_summary.loc[ordered, "p95"], marker="o", label="P95")
    plt.xticks(x, ordered, rotation=25, ha="right")
    plt.ylabel(ylabel)
    plt.title("Mean, Median, and P95 GPU Time")
    plt.grid(True, alpha=0.3)
    plt.legend()
    save_current(args.output_dir, "05_mean_median_p95.png")

    pass_pivot = pass_summary.pivot(
        index="renderer", columns="pass", values="mean"
    ).fillna(0).reindex(renderer_columns)
    plt.figure(figsize=(12, 7))
    bottom = np.zeros(len(pass_pivot))
    for pass_name in pass_pivot.columns:
        values = pass_pivot[pass_name].to_numpy()
        plt.bar(pass_pivot.index, values, bottom=bottom, label=pass_name)
        bottom += values
    plt.ylabel(f"Mean GPU time ({args.unit})")
    plt.title("Mean Pass-Time Breakdown")
    plt.xticks(rotation=25, ha="right")
    plt.grid(True, axis="y", alpha=0.3)
    plt.legend()
    save_current(args.output_dir, "06_mean_pass_breakdown.png")

    plt.figure(figsize=(13, 7))
    for column in deltas.columns:
        if column != "frame":
            plt.plot(deltas["frame"], deltas[column], label=column)
    plt.axhline(0.0)
    plt.xlabel("Frame")
    plt.ylabel(f"Time difference ({args.unit})")
    plt.title("Pairwise GPU-Time Overhead")
    plt.grid(True, alpha=0.3)
    plt.legend(ncol=2)
    save_current(args.output_dir, "07_pairwise_overhead_by_frame.png")

    second_best = np.partition(totals[renderer_columns].to_numpy(), 1, axis=1)[:, 1]
    winning_margin = second_best - winner_time.to_numpy()
    winner_codes, winner_labels = pd.factorize(winner, sort=True)
    plt.figure(figsize=(13, 6))
    plt.scatter(totals["frame"], winner_codes, s=25 + winning_margin * 1200)
    plt.yticks(range(len(winner_labels)), winner_labels)
    plt.xlabel("Frame")
    plt.ylabel("Fastest renderer")
    plt.title("Fastest Renderer by Frame\nMarker size represents margin over second place")
    plt.grid(True, alpha=0.3)
    save_current(args.output_dir, "08_winner_timeline.png")

    plt.figure(figsize=(9, 8))
    image = plt.imshow(correlation.to_numpy(), aspect="auto")
    plt.colorbar(image, label="Pearson correlation")
    plt.xticks(range(len(renderer_columns)), renderer_columns, rotation=35, ha="right")
    plt.yticks(range(len(renderer_columns)), renderer_columns)
    plt.title("Correlation of Per-Frame Renderer Cost")
    for i in range(len(renderer_columns)):
        for j in range(len(renderer_columns)):
            plt.text(j, i, f"{correlation.iloc[i, j]:.2f}", ha="center", va="center")
    save_current(args.output_dir, "09_renderer_correlation.png")

    shared_pass_series = {
        "TVB visibility": data["TVB"]["visibility"],
        "TVB + G-buffer visibility": data["TVB + G-buffer"]["visibility"],
        "Forward prepass depth": data["Forward + Prepass"]["depth_prepass"],
        "Deferred prepass depth": data["Deferred + Prepass"]["depth_prepass"],
        "Deferred lighting": data["Deferred"]["lighting"],
        "Deferred + Prepass lighting": data["Deferred + Prepass"]["lighting"],
        "TVB + G-buffer lighting": data["TVB + G-buffer"]["lighting"],
    }
    plt.figure(figsize=(13, 7))
    for name, series in shared_pass_series.items():
        plt.plot(totals["frame"], series, label=name)
    plt.xlabel("Frame")
    plt.ylabel(pass_ylabel)
    plt.title("Comparable Pass Timings Across Renderer Runs")
    plt.grid(True, alpha=0.3)
    plt.legend(ncol=2)
    save_current(args.output_dir, "10_shared_pass_consistency.png")

    manifest = {
        "inputs": {name: str(path) for name, path in input_files.items()},
        "sample_count": len(totals),
        "frame_min": int(totals["frame"].min()),
        "frame_max": int(totals["frame"].max()),
        "frame_step": int(totals["frame"].diff().dropna().mode().iloc[0]),
        "total_definition": "sum of pass columns",
        "unit_label": args.unit,
    }
    (args.output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2), encoding="utf-8"
    )

    print(summary.to_string(index=False))
    print(f"\nResults written to: {args.output_dir.resolve()}")


if __name__ == "__main__":
    main()
