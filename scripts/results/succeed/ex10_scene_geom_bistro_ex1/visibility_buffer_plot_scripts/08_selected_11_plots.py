#!/usr/bin/env python3
from __future__ import annotations

import argparse
import zipfile
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from common import PASS_COLUMNS, VARIANT_ORDER, ensure_output_dir, load_experiment_csv

SELECTED_PASSES = [
    ("Forward+Prepass", "depth_prepass"),
    ("Forward+Prepass", "forward"),
    ("Deferred", "geometry"),
    ("Deferred", "gbuffer"),
    ("Deferred+Prepass", "geometry"),
    ("TVB", "visibility"),
    ("TVB", "resolve"),
]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("low_alu_csv", type=Path, help="ALU 1,2,4 CSV")
    parser.add_argument("high_alu_csv", type=Path, help="ALU 10,20,40,80,160 CSV")
    parser.add_argument("--output-dir", type=Path, default=Path("alu_selected_11_plots"))
    parser.add_argument("--zip-output", type=Path, default=Path("alu_selected_11_plots.zip"))
    args = parser.parse_args()

    out = ensure_output_dir(args.output_dir.resolve(), clean=True)
    low = load_experiment_csv(args.low_alu_csv.resolve(), [1, 2, 4])
    high = load_experiment_csv(args.high_alu_csv.resolve(), [10, 20, 40, 80, 160])
    created = []

    for alu in [1, 2, 4]:
        plt.figure(figsize=(12, 6))
        for variant in VARIANT_ORDER:
            run = low[(low["alu_calc_count"] == alu) & (low["variant"] == variant)].sort_values("frame")
            plt.plot(run["frame"], run["total"], label=variant)
        plt.title(f"Total GPU Time by Frame — ALU {alu}")
        plt.xlabel("Frame")
        plt.ylabel("GPU time (ms)")
        plt.grid(True, alpha=0.3)
        plt.legend(ncol=2)
        path = out / f"01_total_time_alu_{alu}.png"
        plt.tight_layout()
        plt.savefig(path, dpi=180, bbox_inches="tight")
        plt.close()
        created.append(path)

    for alu in [1, 2, 4]:
        subset = low[low["alu_calc_count"] == alu].copy()
        subset["pass_sum"] = subset[PASS_COLUMNS].sum(axis=1, skipna=True)
        subset["unattributed"] = subset["total"] - subset["pass_sum"]
        median = subset.groupby(["renderer_variant", "variant"], as_index=False)[PASS_COLUMNS + ["unattributed"]].median().set_index("variant").reindex(VARIANT_ORDER).reset_index()
        plt.figure(figsize=(12, 7))
        bottom = np.zeros(len(median))
        for column in PASS_COLUMNS + ["unattributed"]:
            values = median[column].fillna(0).to_numpy()
            if np.allclose(values, 0):
                continue
            plt.bar(median["variant"], values, bottom=bottom, label=column)
            bottom += values
        plt.title(f"Median Pass Breakdown — ALU {alu}")
        plt.xlabel("Renderer variant")
        plt.ylabel("Median GPU time (ms)")
        plt.xticks(rotation=25, ha="right")
        plt.legend(ncol=3)
        path = out / f"02_median_pass_breakdown_alu_{alu}.png"
        plt.tight_layout()
        plt.savefig(path, dpi=180, bbox_inches="tight")
        plt.close()
        created.append(path)

    combined = pd.concat([low[low["alu_calc_count"].isin([1, 2, 4])], high[high["alu_calc_count"].isin([10, 20])]], ignore_index=True)
    for alu in [1, 2, 4, 10, 20]:
        plt.figure(figsize=(14, 8))
        for variant, column in SELECTED_PASSES:
            run = combined[(combined["alu_calc_count"] == alu) & (combined["variant"] == variant)].sort_values("frame")
            if run.empty or column not in run.columns or not run[column].notna().any() or np.allclose(run[column].fillna(0).to_numpy(), 0):
                continue
            plt.plot(run["frame"], run[column], label=f"{variant} | {column}")
        plt.title(f"Selected Pass Times by Frame — ALU {alu}")
        plt.xlabel("Frame")
        plt.ylabel("GPU time (ms)")
        plt.grid(True, alpha=0.3)
        plt.legend(ncol=2)
        path = out / f"03_selected_pass_times_alu_{alu}.png"
        plt.tight_layout()
        plt.savefig(path, dpi=180, bbox_inches="tight")
        plt.close()
        created.append(path)

    if len(created) != 11:
        raise RuntimeError(f"11장이 아니라 {len(created)}장이 생성되었습니다.")
    if args.zip_output.exists():
        args.zip_output.unlink()
    with zipfile.ZipFile(args.zip_output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in created:
            archive.write(path, arcname=path.name)
    print(f"[완료] {args.zip_output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
