#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from common import load_experiment_csv, parse_int_list

REQUESTS = [
    ("Forward+Prepass", ["depth_prepass", "forward"]),
    ("Deferred", ["geometry", "gbuffer"]),
    ("Deferred+Prepass", ["geometry"]),
    ("TVB", ["visibility", "resolve"]),
]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_csv", type=Path)
    parser.add_argument("--sweep-alus", default="10,20,40,80,160")
    parser.add_argument("--plot-alus", default="10,20")
    parser.add_argument("--output", type=Path, default=Path("selected_passes_single_plot.png"))
    args = parser.parse_args()

    data = load_experiment_csv(args.input_csv.resolve(), parse_int_list(args.sweep_alus))
    plot_alus = parse_int_list(args.plot_alus)
    plt.figure(figsize=(15, 8))
    for alu in plot_alus:
        for variant, passes in REQUESTS:
            run = data[(data["variant"] == variant) & (data["alu_calc_count"] == alu)].sort_values("frame")
            for column in passes:
                if column in run.columns and run[column].notna().any() and not np.allclose(run[column].fillna(0).to_numpy(), 0):
                    plt.plot(run["frame"], run[column], label=f"ALU {alu} | {variant} | {column}")
    plt.title("Selected Passes by Frame")
    plt.xlabel("Frame")
    plt.ylabel("GPU time (ms)")
    plt.grid(True, alpha=0.3)
    plt.legend(ncol=2, fontsize=9)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(args.output, dpi=180, bbox_inches="tight")
    plt.close()
    print(f"[완료] {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
