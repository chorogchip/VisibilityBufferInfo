#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd

from common import PASS_COLUMNS, VARIANT_NAMES, ensure_output_dir, load_experiment_csv, parse_int_list


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_csv", type=Path)
    parser.add_argument("--alu-values", default="10,20,40,80,160")
    parser.add_argument("--output-dir", type=Path, default=Path("pass_by_frame"))
    args = parser.parse_args()

    alus = parse_int_list(args.alu_values)
    out = ensure_output_dir(args.output_dir.resolve(), clean=True)
    data = load_experiment_csv(args.input_csv.resolve(), alus)
    rows = []

    for variant_id, variant in VARIANT_NAMES.items():
        for alu in alus:
            run = data[(data["renderer_variant"] == variant_id) & (data["alu_calc_count"] == alu)].sort_values("frame")
            active = [c for c in PASS_COLUMNS if c in run.columns and run[c].notna().any()]
            plt.figure(figsize=(12, 6))
            plt.plot(run["frame"], run["total"], label="total", linewidth=2.4)
            for column in active:
                plt.plot(run["frame"], run[column], label=column, linewidth=1.4)
            plt.title(f"{variant}: Pass Time by Frame — ALU {alu}")
            plt.xlabel("Frame")
            plt.ylabel("GPU time (ms)")
            plt.grid(True, alpha=0.3)
            plt.legend(ncol=2)
            safe = variant.lower().replace("+", "_plus_")
            path = out / f"{safe}_alu_{alu}.png"
            plt.tight_layout()
            plt.savefig(path, dpi=180, bbox_inches="tight")
            plt.close()
            rows.append({"variant": variant, "alu_calc_count": alu, "active_passes": ", ".join(active), "plot_file": path.name})

    pd.DataFrame(rows).to_csv(out / "plot_index.csv", index=False, encoding="utf-8-sig")
    print(f"[완료] {len(rows)}개 -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
