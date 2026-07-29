#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from common import ensure_output_dir, load_experiment_csv, parse_int_list

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
    parser.add_argument("--output-dir", type=Path, default=Path("selected_pass_lines"))
    args = parser.parse_args()

    sweep_alus = parse_int_list(args.sweep_alus)
    plot_alus = parse_int_list(args.plot_alus)
    out = ensure_output_dir(args.output_dir.resolve(), clean=True)
    data = load_experiment_csv(args.input_csv.resolve(), sweep_alus)
    rows = []

    for variant, requested in REQUESTS:
        for alu in plot_alus:
            run = data[(data["variant"] == variant) & (data["alu_calc_count"] == alu)].sort_values("frame")
            available = [c for c in requested if c in run.columns and run[c].notna().any() and not np.allclose(run[c].fillna(0).to_numpy(), 0)]
            plt.figure(figsize=(12, 6))
            for column in available:
                plt.plot(run["frame"], run[column], label=column, linewidth=1.8)
            plt.title(f"{variant}: selected passes — ALU {alu}")
            plt.xlabel("Frame")
            plt.ylabel("GPU time (ms)")
            plt.grid(True, alpha=0.3)
            if available:
                plt.legend(ncol=2)
            safe = variant.lower().replace("+", "_plus_")
            path = out / f"{safe}_alu_{alu}.png"
            plt.tight_layout()
            plt.savefig(path, dpi=180, bbox_inches="tight")
            plt.close()
            rows.append({"variant": variant, "alu_calc_count": alu, "requested": ", ".join(requested), "available": ", ".join(available), "plot_file": path.name})

    pd.DataFrame(rows).to_csv(out / "plot_index.csv", index=False, encoding="utf-8-sig")
    print(f"[완료] {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
