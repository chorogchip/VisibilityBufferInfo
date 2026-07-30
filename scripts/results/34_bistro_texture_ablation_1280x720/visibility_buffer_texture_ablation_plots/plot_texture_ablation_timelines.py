#!/usr/bin/env python3
"""
Create one vertically stacked texture-ablation timeline figure.

Layout:
  top    : texture loading OFF
  bottom : texture loading ON

Each panel overlays renderer variants 8 and 9, including every profiled pass
and total GPU time.

Usage:
    python plot_texture_ablation_timelines.py \
        --input 34_bistro_texture_ablation_1280x720.zip \
        --output-dir plots
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
from PIL import Image


VARIANTS = {
    8: {
        "renderer": "DonutDeferredPrepass",
        "short": "V8",
        "label": "Variant 8 · Deferred + Prepass",
        "linestyle": "-",
        "passes": ("depth_prepass", "geometry", "lighting", "tonemap"),
    },
    9: {
        "renderer": "DonutVisGBuffer",
        "short": "V9",
        "label": "Variant 9 · Visibility + G-buffer",
        "linestyle": "--",
        "passes": (
            "visibility",
            "visutil_histogram",
            "visutil_prefix",
            "visutil_flatten",
            "gbuffer",
            "lighting",
            "tonemap",
        ),
    },
}

# Related passes use related hues.
PASS_STYLE = {
    "total": ("Total", "#202124", 2.8),
    "depth_prepass": ("Depth prepass", "#4C78A8", 1.7),
    "visibility": ("Visibility", "#72B7B2", 1.7),
    "geometry": ("G-buffer make", "#59A14F", 1.7),
    "gbuffer": ("G-buffer", "#8CD17D", 1.7),
    "lighting": ("Lighting", "#F28E2B", 1.55),
    "tonemap": ("Tonemap", "#B279A2", 1.55),
    "visutil_histogram": ("Visutil histogram", "#E15759", 1.35),
    "visutil_prefix": ("Visutil prefix", "#FF9D9A", 1.35),
    "visutil_flatten": ("Visutil flatten", "#D37295", 1.35),
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
                f"Variant {variant}: expected {VARIANTS[variant]['renderer']}, "
                f"got {renderer}"
            )

        texture_on = bool(int(row["to-load-texture"]))
        width = int(row["window-width"])
        height = int(row["window-height"])
        current_resolution = (width, height)
        if resolution is None:
            resolution = current_resolution
        elif current_resolution != resolution:
            raise ValueError(
                f"Expected one resolution, got {resolution} and "
                f"{current_resolution}"
            )

        run_index = int(row["runner_run_index"])
        matches = sorted(runs_dir.glob(f"run_{run_index:05d}.csv_*_result.csv"))
        if len(matches) != 1:
            raise ValueError(f"Run {run_index}: found {matches}")

        profile = pd.read_csv(matches[0], encoding="utf-8-sig")
        required = {
            "frame",
            "total",
            "index_count",
            *VARIANTS[variant]["passes"],
        }
        missing = required - set(profile.columns)
        if missing:
            raise ValueError(f"{matches[0].name}: missing {sorted(missing)}")

        numeric = list(required - {"frame"})
        if profile[numeric].isna().any().any():
            raise ValueError(f"{matches[0].name}: missing numeric data")
        if (profile[numeric] < 0).any().any():
            raise ValueError(f"{matches[0].name}: negative timing data")

        key = (texture_on, variant)
        if key in profiles:
            raise ValueError(f"Duplicate profile: {key}")
        profiles[key] = profile

    if resolution is None:
        raise ValueError("No valid profiles found.")

    for texture_on in (False, True):
        for variant in VARIANTS:
            if (texture_on, variant) not in profiles:
                raise ValueError(
                    f"Missing texture={texture_on}, variant={variant}"
                )

        p8 = profiles[(texture_on, 8)]
        p9 = profiles[(texture_on, 9)]
        if p8["frame"].tolist() != p9["frame"].tolist():
            raise ValueError(
                f"Frame timelines differ for texture={texture_on}"
            )
        if p8["index_count"].tolist() != p9["index_count"].tolist():
            raise ValueError(
                f"Workload differs for texture={texture_on}"
            )

    return profiles, resolution


def plot_texture_state(
    profiles: dict[tuple[bool, int], pd.DataFrame],
    texture_on: bool,
    resolution: tuple[int, int],
    output_path: Path,
    shared_y_limit: float,
    dpi: int,
) -> None:
    width, height = resolution
    state_label = "ON" if texture_on else "OFF"

    figure = plt.figure(figsize=(16, 7.3))
    axis = figure.add_axes((0.065, 0.13, 0.92, 0.73))

    for variant in (8, 9):
        config = VARIANTS[variant]
        profile = profiles[(texture_on, variant)]
        for pass_name in ("total", *config["passes"]):
            pass_label, color, linewidth = PASS_STYLE[pass_name]
            axis.plot(
                profile["frame"],
                profile[pass_name],
                color=color,
                linestyle=config["linestyle"],
                linewidth=linewidth,
                alpha=1.0 if pass_name == "total" else 0.92,
                label=f"{config['short']} {pass_label}",
                zorder=20 if pass_name == "total" else 5,
            )

    p8 = profiles[(texture_on, 8)]
    p9 = profiles[(texture_on, 9)]
    frame_step = int(p8["frame"].diff().dropna().mode().iloc[0])

    figure.suptitle(
        f"Texture loading {state_label} · Variant 8 vs Variant 9",
        fontsize=17,
        fontweight="bold",
        y=0.975,
    )
    figure.text(
        0.5,
        0.918,
        (
            f"{width}×{height} · {len(p8)} profile windows · "
            f"{frame_step}-frame windows · "
            f"V8 total mean {p8['total'].mean():.4f} ms · "
            f"V9 total mean {p9['total'].mean():.4f} ms · "
            "solid = V8, dashed = V9"
        ),
        ha="center",
        va="center",
        fontsize=10.5,
        color="#444444",
    )

    axis.set_xlim(p8["frame"].min(), p8["frame"].max())
    axis.set_ylim(0, shared_y_limit)
    axis.set_xlabel("Frame timeline")
    axis.set_ylabel("GPU time (ms)")
    axis.grid(True, alpha=0.24, linewidth=0.8)
    axis.legend(
        loc="upper left",
        ncol=4,
        frameon=True,
        fontsize=8.7,
        columnspacing=1.0,
        handlelength=2.8,
    )

    figure.savefig(output_path, dpi=dpi)
    plt.close(figure)


def compose_vertical(
    top_path: Path,
    bottom_path: Path,
    output_path: Path,
) -> None:
    with Image.open(top_path) as top_source, Image.open(bottom_path) as bottom_source:
        top = top_source.convert("RGB")
        bottom = bottom_source.convert("RGB")
        target_width = max(top.width, bottom.width)

        if top.width != target_width:
            top = top.resize(
                (target_width, round(top.height * target_width / top.width)),
                Image.Resampling.LANCZOS,
            )
        if bottom.width != target_width:
            bottom = bottom.resize(
                (target_width, round(bottom.height * target_width / bottom.width)),
                Image.Resampling.LANCZOS,
            )

        gap = 16
        canvas = Image.new(
            "RGB",
            (target_width, top.height + gap + bottom.height),
            "white",
        )
        canvas.paste(top, (0, 0))
        canvas.paste(bottom, (0, top.height + gap))
        canvas.save(output_path, quality=96)


def write_summary(
    profiles: dict[tuple[bool, int], pd.DataFrame],
    path: Path,
) -> None:
    rows = []
    for (texture_on, variant), profile in sorted(profiles.items()):
        for pass_name in ("total", *VARIANTS[variant]["passes"]):
            series = profile[pass_name]
            rows.append({
                "texture_load": "on" if texture_on else "off",
                "variant": variant,
                "renderer": VARIANTS[variant]["renderer"],
                "pass": pass_name,
                "mean_ms": series.mean(),
                "median_ms": series.median(),
                "p90_ms": series.quantile(0.90),
                "p99_ms": series.quantile(0.99),
                "min_ms": series.min(),
                "max_ms": series.max(),
            })
    pd.DataFrame(rows).to_csv(path, index=False, float_format="%.6f")


def main() -> int:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="texture_ablation_plot_") as tmp:
        root = extract_or_use(args.input, Path(tmp))
        profiles, resolution = load_profiles(root)

    plt.style.use("seaborn-v0_8-whitegrid")

    shared_y_limit = (
        max(
            profile["total"].max()
            for profile in profiles.values()
        )
        * 1.13
    )

    off_path = args.output_dir / "texture_off_variants_8_9_timeline.png"
    on_path = args.output_dir / "texture_on_variants_8_9_timeline.png"

    plot_texture_state(
        profiles,
        False,
        resolution,
        off_path,
        shared_y_limit,
        args.dpi,
    )
    plot_texture_state(
        profiles,
        True,
        resolution,
        on_path,
        shared_y_limit,
        args.dpi,
    )

    combined_path = (
        args.output_dir
        / "texture_off_on_variant_8_9_pass_timeline.png"
    )
    compose_vertical(off_path, on_path, combined_path)

    summary_path = args.output_dir / "texture_ablation_pass_summary.csv"
    write_summary(profiles, summary_path)

    manifest = {
        "resolution": f"{resolution[0]}x{resolution[1]}",
        "layout": {
            "top": "texture load off",
            "bottom": "texture load on",
        },
        "each_panel_contains": [
            "variant 8 total and all passes",
            "variant 9 total and all passes",
        ],
        "variant_styles": {
            "8": "solid",
            "9": "dashed",
        },
        "shared_y_axis_range": True,
        "files": [
            off_path.name,
            on_path.name,
            combined_path.name,
            summary_path.name,
        ],
    }
    (args.output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2),
        encoding="utf-8",
    )

    print(combined_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
