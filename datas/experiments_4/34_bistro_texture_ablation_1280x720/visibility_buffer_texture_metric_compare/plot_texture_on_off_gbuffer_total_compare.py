#!/usr/bin/env python3
"""
Plot one aggregate comparison line chart from the texture ablation dataset.

X axis:
    texture OFF, texture ON

Series (4 total):
    - Variant 8 G-buffer make (geometry pass)
    - Variant 8 Total
    - Variant 9 G-buffer
    - Variant 9 Total
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
import pandas as pd


VARIANTS = {
    8: {
        "renderer": "DonutDeferredPrepass",
        "gbuffer_like_pass": "geometry",
        "gbuffer_label": "V8 G-buffer make",
        "linestyle": "-",
    },
    9: {
        "renderer": "DonutVisGBuffer",
        "gbuffer_like_pass": "gbuffer",
        "gbuffer_label": "V9 G-buffer",
        "linestyle": "--",
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

    runs_dirs = [
        path for path in root.iterdir()
        if path.is_dir() and path.name.endswith("_runs")
    ]
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
        needed = {
            "frame",
            "total",
            VARIANTS[variant]["gbuffer_like_pass"],
        }
        missing = needed - set(profile.columns)
        if missing:
            raise ValueError(f"{matches[0].name}: missing {sorted(missing)}")

        numeric = list(needed - {"frame"})
        if profile[numeric].isna().any().any():
            raise ValueError(f"{matches[0].name}: missing numeric data")
        if (profile[numeric] < 0).any().any():
            raise ValueError(f"{matches[0].name}: negative timing data")

        key = (texture_on, variant)
        profiles[key] = profile

    if resolution is None:
        raise ValueError("No valid profiles found.")

    for texture_on in TEXTURE_ORDER:
        for variant in VARIANTS:
            if (texture_on, variant) not in profiles:
                raise ValueError(f"Missing texture={texture_on}, variant={variant}")

    return profiles, resolution


def build_summary(
    profiles: dict[tuple[bool, int], pd.DataFrame],
) -> pd.DataFrame:
    rows = []
    for texture_on in TEXTURE_ORDER:
        for variant, config in VARIANTS.items():
            profile = profiles[(texture_on, variant)]
            gbuffer_pass = config["gbuffer_like_pass"]
            rows.append(
                {
                    "texture_load": "on" if texture_on else "off",
                    "variant": variant,
                    "renderer": config["renderer"],
                    "metric": config["gbuffer_label"],
                    "pass_name": gbuffer_pass,
                    "mean_ms": profile[gbuffer_pass].mean(),
                    "median_ms": profile[gbuffer_pass].median(),
                    "p90_ms": profile[gbuffer_pass].quantile(0.90),
                    "p99_ms": profile[gbuffer_pass].quantile(0.99),
                }
            )
            rows.append(
                {
                    "texture_load": "on" if texture_on else "off",
                    "variant": variant,
                    "renderer": config["renderer"],
                    "metric": f"V{variant} Total",
                    "pass_name": "total",
                    "mean_ms": profile["total"].mean(),
                    "median_ms": profile["total"].median(),
                    "p90_ms": profile["total"].quantile(0.90),
                    "p99_ms": profile["total"].quantile(0.99),
                }
            )
    return pd.DataFrame(rows)


def plot(summary: pd.DataFrame, resolution: tuple[int, int], output_path: Path, dpi: int) -> None:
    width, height = resolution
    figure = plt.figure(figsize=(12.5, 7.2))
    axis = figure.add_axes((0.095, 0.14, 0.87, 0.72))

    x = [0, 1]
    labels = [TEXTURE_LABELS[t] for t in TEXTURE_ORDER]

    series_specs = [
        ("V8 G-buffer make", "#59A14F", "-", "o"),
        ("V8 Total", "#202124", "-", "o"),
        ("V9 G-buffer", "#8CD17D", "--", "s"),
        ("V9 Total", "#6B6B6B", "--", "s"),
    ]

    for metric, color, linestyle, marker in series_specs:
        values = []
        for texture_on in TEXTURE_ORDER:
            row = summary[
                (summary["texture_load"] == ("on" if texture_on else "off"))
                & (summary["metric"] == metric)
            ]
            if len(row) != 1:
                raise ValueError(f"Expected exactly one row for {metric}, texture={texture_on}")
            values.append(float(row.iloc[0]["mean_ms"]))

        axis.plot(
            x,
            values,
            color=color,
            linestyle=linestyle,
            linewidth=2.2,
            marker=marker,
            markersize=8,
            label=metric,
        )
        for xi, yi in zip(x, values):
            axis.text(
                xi,
                yi,
                f"{yi:.4f}",
                ha="center",
                va="bottom",
                fontsize=9.5,
            )

    figure.suptitle(
        "Texture ON/OFF comparison · G-buffer-like pass and total",
        fontsize=16.5,
        fontweight="bold",
        y=0.975,
    )
    figure.text(
        0.5,
        0.915,
        (
            f"{width}×{height} · x = texture load setting · "
            "4 series = V8 G-buffer make, V8 Total, V9 G-buffer, V9 Total"
        ),
        ha="center",
        va="center",
        fontsize=10.3,
        color="#444444",
    )

    axis.set_xticks(x, labels)
    axis.set_ylabel("Mean GPU time (ms)")
    axis.set_xlabel("Texture load setting")
    axis.grid(True, alpha=0.25)
    axis.legend(loc="upper left", frameon=True)
    axis.set_xlim(-0.15, 1.15)

    ymax = summary["mean_ms"].max() * 1.18
    axis.set_ylim(0, ymax)

    figure.savefig(output_path, dpi=dpi)
    plt.close(figure)


def main() -> int:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="texture_metric_compare_") as tmp:
        root = extract_or_use(args.input, Path(tmp))
        profiles, resolution = load_profiles(root)

    plt.style.use("seaborn-v0_8-whitegrid")

    summary = build_summary(profiles)
    summary_path = args.output_dir / "texture_on_off_gbuffer_total_summary.csv"
    summary.to_csv(summary_path, index=False, float_format="%.6f")

    plot_path = args.output_dir / "texture_on_off_gbuffer_total_compare.png"
    plot(summary, resolution, plot_path, args.dpi)

    manifest = {
        "resolution": f"{resolution[0]}x{resolution[1]}",
        "x_axis": ["Texture OFF", "Texture ON"],
        "series": [
            "V8 G-buffer make",
            "V8 Total",
            "V9 G-buffer",
            "V9 Total",
        ],
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
