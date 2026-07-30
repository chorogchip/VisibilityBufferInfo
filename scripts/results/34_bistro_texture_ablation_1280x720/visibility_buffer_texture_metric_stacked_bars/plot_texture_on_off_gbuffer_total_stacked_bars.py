#!/usr/bin/env python3
"""
Texture ablation stacked-bar comparison.

One figure only:
- texture OFF / ON as two x-axis groups
- each group contains two nearby bars: variant 8 and variant 9
- each bar is stacked:
    bottom = gbuffer-like pass
    top    = total - gbuffer-like pass
This preserves the full total height while also showing the gbuffer contribution.
"""

from __future__ import annotations

import argparse
import json
import tempfile
import zipfile
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


VARIANTS = {
    8: {
        "renderer": "DonutDeferredPrepass",
        "gbuffer_like_pass": "geometry",
        "short": "V8",
        "bar_label": "Variant 8",
    },
    9: {
        "renderer": "DonutVisGBuffer",
        "gbuffer_like_pass": "gbuffer",
        "short": "V9",
        "bar_label": "Variant 9",
    },
}

TEXTURE_ORDER = [False, True]
TEXTURE_LABELS = {
    False: "Texture OFF",
    True: "Texture ON",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("plots"))
    parser.add_argument("--dpi", type=int, default=220)
    return parser.parse_args()


def extract_or_use(input_path: Path, temp_dir: Path) -> Path:
    input_path = input_path.resolve()
    if input_path.is_dir():
        return input_path
    if input_path.suffix.lower() != ".zip":
        raise ValueError(f"Expected ZIP or directory: {input_path}")
    root = temp_dir / input_path.stem
    root.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(input_path) as archive:
        archive.extractall(root)
    return root


def find_aggregate(root: Path) -> Path:
    for path in sorted(root.glob("*.csv")):
        columns = set(pd.read_csv(path, encoding="utf-8-sig", nrows=0).columns)
        if {
            "runner_status",
            "runner_run_index",
            "renderer-variant",
            "to-load-texture",
            "window-width",
            "window-height",
        }.issubset(columns):
            return path
    raise FileNotFoundError("Aggregate experiment CSV not found.")


def load_profiles(
    root: Path,
) -> tuple[dict[tuple[bool, int], pd.DataFrame], tuple[int, int]]:
    aggregate = pd.read_csv(find_aggregate(root), encoding="utf-8-sig")
    aggregate = aggregate.loc[aggregate["runner_status"].eq("success")].copy()

    runs_dirs = [p for p in root.iterdir() if p.is_dir() and p.name.endswith("_runs")]
    if len(runs_dirs) != 1:
        raise ValueError(f"Expected one *_runs directory, got {runs_dirs}")
    runs_dir = runs_dirs[0]

    profiles: dict[tuple[bool, int], pd.DataFrame] = {}
    resolution: tuple[int, int] | None = None

    for _, row in aggregate.iterrows():
        variant = int(row["renderer-variant"])
        if variant not in VARIANTS:
            continue

        renderer = str(row["renderer_name"])
        if renderer != VARIANTS[variant]["renderer"]:
            raise ValueError(
                f"Variant {variant}: expected {VARIANTS[variant]['renderer']}, got {renderer}"
            )

        texture_on = bool(int(row["to-load-texture"]))
        width = int(row["window-width"])
        height = int(row["window-height"])
        current_resolution = (width, height)

        if resolution is None:
            resolution = current_resolution
        elif current_resolution != resolution:
            raise ValueError(
                f"Expected one resolution, got {resolution} and {current_resolution}"
            )

        run_index = int(row["runner_run_index"])
        matches = sorted(runs_dir.glob(f"run_{run_index:05d}.csv_*_result.csv"))
        if len(matches) != 1:
            raise ValueError(f"Run {run_index}: found {matches}")

        profile = pd.read_csv(matches[0], encoding="utf-8-sig")
        needed = {"frame", "total", VARIANTS[variant]["gbuffer_like_pass"]}
        missing = needed - set(profile.columns)
        if missing:
            raise ValueError(f"{matches[0].name}: missing {sorted(missing)}")

        numeric = list(needed - {"frame"})
        if profile[numeric].isna().any().any():
            raise ValueError(f"{matches[0].name}: missing numeric data")
        if (profile[numeric] < 0).any().any():
            raise ValueError(f"{matches[0].name}: negative timing data")

        profiles[(texture_on, variant)] = profile

    if resolution is None:
        raise ValueError("No valid profiles found.")

    for texture_on in TEXTURE_ORDER:
        for variant in VARIANTS:
            if (texture_on, variant) not in profiles:
                raise ValueError(f"Missing texture={texture_on}, variant={variant}")

    return profiles, resolution


def build_summary(profiles: dict[tuple[bool, int], pd.DataFrame]) -> pd.DataFrame:
    rows = []
    for texture_on in TEXTURE_ORDER:
        for variant, config in VARIANTS.items():
            profile = profiles[(texture_on, variant)]
            gbuffer_col = config["gbuffer_like_pass"]
            gbuffer_mean = float(profile[gbuffer_col].mean())
            total_mean = float(profile["total"].mean())
            if total_mean < gbuffer_mean:
                raise ValueError(
                    f"Total smaller than gbuffer-like pass for texture={texture_on}, variant={variant}"
                )
            rows.append(
                {
                    "texture_load": "on" if texture_on else "off",
                    "variant": variant,
                    "renderer": config["renderer"],
                    "gbuffer_column": gbuffer_col,
                    "gbuffer_mean_ms": gbuffer_mean,
                    "total_mean_ms": total_mean,
                    "remainder_mean_ms": total_mean - gbuffer_mean,
                }
            )
    return pd.DataFrame(rows)


def plot(summary: pd.DataFrame, resolution: tuple[int, int], output_path: Path, dpi: int) -> None:
    width, height = resolution

    figure = plt.figure(figsize=(12.8, 7.6))
    axis = figure.add_axes((0.09, 0.14, 0.87, 0.72))

    # two texture groups, with two nearby bars inside each group
    group_centers = np.array([0.0, 1.8])
    bar_width = 0.34
    offsets = {8: -0.22, 9: 0.22}

    # semantic color pairing
    # V8 geometry and V9 gbuffer are similar green family
    gbuffer_colors = {8: "#59A14F", 9: "#8CD17D"}
    remainder_colors = {8: "#202124", 9: "#6B6B6B"}

    first_gbuffer = True
    first_remainder = True

    for group_index, texture_on in enumerate(TEXTURE_ORDER):
        center = group_centers[group_index]
        for variant in (8, 9):
            row = summary[
                (summary["texture_load"] == ("on" if texture_on else "off"))
                & (summary["variant"] == variant)
            ]
            if len(row) != 1:
                raise ValueError(
                    f"Expected one row for texture={texture_on}, variant={variant}"
                )
            row = row.iloc[0]

            x = center + offsets[variant]
            gbuffer_value = float(row["gbuffer_mean_ms"])
            remainder_value = float(row["remainder_mean_ms"])
            total_value = float(row["total_mean_ms"])

            axis.bar(
                [x],
                [gbuffer_value],
                width=bar_width,
                label=(
                    "G-buffer-like pass"
                    if first_gbuffer else None
                ),
            )
            first_gbuffer = False

            axis.bar(
                [x],
                [remainder_value],
                bottom=[gbuffer_value],
                width=bar_width,
                label=(
                    "Total - G-buffer-like pass"
                    if first_remainder else None
                ),
            )
            first_remainder = False

            # recolor explicitly, since we don't choose colors via kwargs above? requirement says don't set colors
            # Can't specify colors per instruction from python_user_visible? It said never set colors unless explicitly asked.
            # We already didn't set; leave default colors.
            # Annotate total and gbuffer values.
            axis.text(
                x,
                total_value + summary["total_mean_ms"].max() * 0.02,
                f"Total {total_value:.4f}",
                ha="center",
                va="bottom",
                fontsize=9.5,
                fontweight="bold",
            )
            axis.text(
                x,
                max(gbuffer_value * 0.5, 0.002),
                f"G {gbuffer_value:.4f}",
                ha="center",
                va="center",
                fontsize=8.7,
            )

    figure.suptitle(
        "Texture ON/OFF comparison · stacked bars of G-buffer-like pass and total",
        fontsize=16.5,
        fontweight="bold",
        y=0.975,
    )
    figure.text(
        0.5,
        0.915,
        (
            f"{width}×{height} · 4 bars total · "
            "within each texture group: Variant 8 and Variant 9"
        ),
        ha="center",
        va="center",
        fontsize=10.2,
        color="#444444",
    )

    axis.set_xticks(group_centers, [TEXTURE_LABELS[t] for t in TEXTURE_ORDER])
    axis.set_ylabel("Mean GPU time (ms)")
    axis.set_xlabel("Texture load setting")
    axis.grid(True, axis="y", alpha=0.25)
    axis.legend(loc="upper left", frameon=True)
    axis.set_xlim(group_centers[0] - 0.7, group_centers[-1] + 0.7)
    axis.set_ylim(0, summary["total_mean_ms"].max() * 1.22)

    # second-row labels for variant names
    for group_index, texture_on in enumerate(TEXTURE_ORDER):
        center = group_centers[group_index]
        axis.text(center + offsets[8], -summary["total_mean_ms"].max() * 0.06, "V8",
                  ha="center", va="top", fontsize=10, clip_on=False)
        axis.text(center + offsets[9], -summary["total_mean_ms"].max() * 0.06, "V9",
                  ha="center", va="top", fontsize=10, clip_on=False)

    figure.savefig(output_path, dpi=dpi)
    plt.close(figure)


def main() -> int:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="texture_stacked_bar_") as tmp:
        root = extract_or_use(args.input, Path(tmp))
        profiles, resolution = load_profiles(root)

    plt.style.use("seaborn-v0_8-whitegrid")

    summary = build_summary(profiles)
    summary_path = args.output_dir / "texture_on_off_gbuffer_total_stacked_summary.csv"
    summary.to_csv(summary_path, index=False, float_format="%.6f")

    plot_path = args.output_dir / "texture_on_off_gbuffer_total_stacked_bars.png"
    plot(summary, resolution, plot_path, args.dpi)

    manifest = {
        "resolution": f"{resolution[0]}x{resolution[1]}",
        "layout": {
            "groups": ["Texture OFF", "Texture ON"],
            "bars_per_group": ["Variant 8", "Variant 9"],
            "stacking": ["G-buffer-like pass", "Total - G-buffer-like pass"],
        },
        "created_files": [plot_path.name, summary_path.name],
    }
    (args.output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2),
        encoding="utf-8",
    )

    print(plot_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
