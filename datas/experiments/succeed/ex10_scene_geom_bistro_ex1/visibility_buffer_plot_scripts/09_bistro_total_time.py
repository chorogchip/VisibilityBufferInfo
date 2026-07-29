#!/usr/bin/env python3
from __future__ import annotations

import argparse
import zipfile
from pathlib import Path

import matplotlib.pyplot as plt

from common import VARIANT_ORDER, ensure_output_dir, load_experiment_csv


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_csv", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("bistro_total_time_plots"))
    parser.add_argument("--zip-output", type=Path, default=Path("bistro_total_time_plots.zip"))
    parser.add_argument("--scene-name", default="Bistro Exterior")
    args = parser.parse_args()

    out = ensure_output_dir(args.output_dir.resolve(), clean=True)
    data = load_experiment_csv(args.input_csv.resolve(), [1, 5, 10, 20])
    created = []

    for alu in [1, 5, 10, 20]:
        plt.figure(figsize=(13, 6.5))
        for variant in VARIANT_ORDER:
            run = data[(data["alu_calc_count"] == alu) & (data["variant"] == variant)].sort_values("frame")
            plt.plot(run["frame"], run["total"], label=variant, linewidth=1.35)
        plt.title(f"{args.scene_name}: Total GPU Time by Frame — ALU {alu}")
        plt.xlabel("Frame")
        plt.ylabel("GPU time (ms)")
        plt.grid(True, alpha=0.3)
        plt.legend(ncol=2)
        path = out / f"bistro_total_time_alu_{alu}.png"
        plt.tight_layout()
        plt.savefig(path, dpi=180, bbox_inches="tight")
        plt.close()
        created.append(path)

    if args.zip_output.exists():
        args.zip_output.unlink()
    with zipfile.ZipFile(args.zip_output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in created:
            archive.write(path, arcname=path.name)
    print(f"[완료] {args.zip_output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
