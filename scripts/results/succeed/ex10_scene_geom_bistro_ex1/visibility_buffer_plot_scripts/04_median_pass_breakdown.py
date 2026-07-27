#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from common import PASS_COLUMNS, VARIANT_ORDER, ensure_output_dir, load_experiment_csv, parse_int_list


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_csv", type=Path)
    parser.add_argument("--sweep-alus", default="10,20,40,80,160")
    parser.add_argument("--plot-alus", default="10,40,160")
    parser.add_argument("--output-dir", type=Path, default=Path("median_pass_breakdown"))
    args = parser.parse_args()

    sweep_alus = parse_int_list(args.sweep_alus)
    plot_alus = parse_int_list(args.plot_alus)
    out = ensure_output_dir(args.output_dir.resolve(), clean=True)
    data = load_experiment_csv(args.input_csv.resolve(), sweep_alus)
    data["pass_sum"] = data[PASS_COLUMNS].sum(axis=1, skipna=True)
    data["unattributed"] = data["total"] - data["pass_sum"]
    summary = data.groupby(["renderer_variant", "variant", "alu_calc_count"], as_index=False)[PASS_COLUMNS + ["total", "pass_sum", "unattributed"]].median()
    summary[summary["alu_calc_count"].isin(plot_alus)].to_csv(out / "pass_median_summary.csv", index=False, encoding="utf-8-sig")

    for alu in plot_alus:
        plot_data = summary[summary["alu_calc_count"] == alu].set_index("variant").reindex(VARIANT_ORDER).reset_index()
        plt.figure(figsize=(12, 7))
        bottom = np.zeros(len(plot_data))
        for column in PASS_COLUMNS + ["unattributed"]:
            values = plot_data[column].fillna(0).to_numpy()
            if np.allclose(values, 0):
                continue
            plt.bar(plot_data["variant"], values, bottom=bottom, label=column)
            bottom += values
        plt.title(f"Median Pass Breakdown — ALU {alu}")
        plt.xlabel("Renderer variant")
        plt.ylabel("Median GPU time (ms)")
        plt.xticks(rotation=25, ha="right")
        plt.grid(True, axis="y", alpha=0.3)
        plt.legend(ncol=3)
        plt.tight_layout()
        plt.savefig(out / f"median_pass_breakdown_alu_{alu}.png", dpi=180, bbox_inches="tight")
        plt.close()

    print(f"[완료] {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
