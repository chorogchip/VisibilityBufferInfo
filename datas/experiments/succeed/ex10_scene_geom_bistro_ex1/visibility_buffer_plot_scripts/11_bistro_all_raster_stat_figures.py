#!/usr/bin/env python3
from __future__ import annotations

import argparse
import zipfile
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd

from common import align_raster_stats, ensure_output_dir, load_experiment_csv


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("result_csv", type=Path)
    parser.add_argument("raster_stats_csv", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("bistro_raster_stat_vs_gpu_plots"))
    parser.add_argument("--zip-output", type=Path, default=Path("bistro_raster_stat_vs_gpu_plots.zip"))
    parser.add_argument("--window-frames", type=int, default=10)
    args = parser.parse_args()

    out = ensure_output_dir(args.output_dir.resolve(), clean=True)
    results = load_experiment_csv(args.result_csv.resolve(), [1, 5, 10, 20])
    raw_stats = pd.read_csv(args.raster_stats_csv.resolve())
    stats = align_raster_stats(raw_stats, args.window_frames)
    stat_columns = [column for column in raw_stats.columns if column != "frame"]
    variants = ["Forward", "Forward+Prepass", "TVB"]
    target_alus = [5, 10, 20]
    gpu = results[results["alu_calc_count"].isin(target_alus) & results["variant"].isin(variants)][["frame", "alu_calc_count", "variant", "total"]]
    created = []

    for stat_name in stat_columns:
        fig, axes = plt.subplots(3, 1, figsize=(15, 12), sharex=True)
        for axis, alu in zip(axes, target_alus):
            wide = gpu[gpu["alu_calc_count"] == alu].pivot(index="frame", columns="variant", values="total").reset_index()
            merged = wide.merge(stats[["frame", stat_name]], on="frame", how="inner")
            handles, labels = [], []
            for variant in variants:
                line = axis.plot(merged["frame"], merged[variant], label=variant)[0]
                handles.append(line)
                labels.append(variant)
            axis.set_ylabel("GPU time (ms)")
            axis.set_title(f"ALU {alu}")
            axis.grid(True, alpha=0.3)
            second = axis.twinx()
            stat_line = second.plot(merged["frame"], merged[stat_name], linestyle="--", label=stat_name)[0]
            second.set_ylabel(stat_name)
            axis.legend(handles + [stat_line], labels + [stat_name], ncol=2)
        axes[-1].set_xlabel("Profile frame")
        fig.suptitle(f"Bistro Exterior — {stat_name} vs GPU Time\nALU 5, 10, 20")
        fig.tight_layout(rect=[0, 0, 1, 0.96])
        path = out / f"{stat_name}_vs_gpu_time.png"
        fig.savefig(path, dpi=180, bbox_inches="tight")
        plt.close(fig)
        created.append(path)

    pd.DataFrame({"stat_name": stat_columns, "plot_file": [path.name for path in created]}).to_csv(out / "plot_index.csv", index=False, encoding="utf-8-sig")
    if args.zip_output.exists():
        args.zip_output.unlink()
    with zipfile.ZipFile(args.zip_output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in created:
            archive.write(path, arcname=path.name)
        archive.write(out / "plot_index.csv", arcname="plot_index.csv")
    print(f"[완료] {args.zip_output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
