#!/usr/bin/env python3
"""
Create one stacked comparison figure:
  top    : small resolution
  bottom : large resolution

Each resolution chart overlays renderer variants 8 and 9, including every
profiled pass and total GPU time.

Usage:
    python plot_resolution_variant_timelines.py \
        --input 33_bistro_pbr_resolution_renderer_compare.zip \
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

# Related operations use related hues:
# depth_prepass <-> visibility: blue/teal
# geometry (G-buffer make) <-> gbuffer: green
# identical passes use identical colors; variant is distinguished by line style.
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
            "window-width",
            "window-height",
        }.issubset(columns):
            return path
    raise FileNotFoundError("Aggregate experiment CSV not found.")


def load_profiles(root: Path) -> dict[tuple[int, int, int], pd.DataFrame]:
    aggregate = pd.read_csv(find_aggregate(root), encoding="utf-8-sig")
    aggregate = aggregate.loc[aggregate["runner_status"].eq("success")].copy()

    runs_dirs = [p for p in root.iterdir() if p.is_dir() and p.name.endswith("_runs")]
    if len(runs_dirs) != 1:
        raise ValueError(f"Expected one *_runs directory, got {runs_dirs}")
    runs_dir = runs_dirs[0]

    profiles: dict[tuple[int, int, int], pd.DataFrame] = {}
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

        width = int(row["window-width"])
        height = int(row["window-height"])
        run_index = int(row["runner_run_index"])
        matches = sorted(runs_dir.glob(f"run_{run_index:05d}.csv_*_result.csv"))
        if len(matches) != 1:
            raise ValueError(f"Run {run_index}: found {matches}")

        profile = pd.read_csv(matches[0], encoding="utf-8-sig")
        needed = {"frame", "total", "index_count", *VARIANTS[variant]["passes"]}
        missing = needed - set(profile.columns)
        if missing:
            raise ValueError(f"{matches[0].name}: missing {sorted(missing)}")

        numeric = list(needed - {"frame"})
        if profile[numeric].isna().any().any():
            raise ValueError(f"{matches[0].name}: missing numeric data")
        if (profile[numeric] < 0).any().any():
            raise ValueError(f"{matches[0].name}: negative timing data")

        profiles[(width, height, variant)] = profile

    resolutions = sorted({(w, h) for w, h, _ in profiles}, key=lambda x: x[0] * x[1])
    for resolution in resolutions:
        p8 = profiles[(*resolution, 8)]
        p9 = profiles[(*resolution, 9)]
        if p8["frame"].tolist() != p9["frame"].tolist():
            raise ValueError(f"Frame timelines differ at {resolution}")
        if p8["index_count"].tolist() != p9["index_count"].tolist():
            raise ValueError(f"Workload differs at {resolution}")
    return profiles


def plot_resolution(
    profiles: dict[tuple[int, int, int], pd.DataFrame],
    resolution: tuple[int, int],
    output_path: Path,
    dpi: int,
) -> None:
    width, height = resolution
    figure = plt.figure(figsize=(16, 7.3))
    axis = figure.add_axes((0.065, 0.13, 0.92, 0.73))

    for variant in (8, 9):
        config = VARIANTS[variant]
        profile = profiles[(width, height, variant)]
        for pass_name in ("total", *config["passes"]):
            pass_label, color, line_width = PASS_STYLE[pass_name]
            axis.plot(
                profile["frame"],
                profile[pass_name],
                color=color,
                linestyle=config["linestyle"],
                linewidth=line_width,
                alpha=1.0 if pass_name == "total" else 0.92,
                label=f"{config['short']} {pass_label}",
                zorder=20 if pass_name == "total" else 5,
            )

    p8 = profiles[(width, height, 8)]
    p9 = profiles[(width, height, 9)]
    frame_step = int(p8["frame"].diff().dropna().mode().iloc[0])

    figure.suptitle(
        f"{width}×{height} · Variant 8 vs Variant 9 pass timeline",
        fontsize=17,
        fontweight="bold",
        y=0.975,
    )
    figure.text(
        0.5,
        0.918,
        (
            f"{len(p8)} profile windows · {frame_step}-frame windows · "
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
    max_total = max(p8["total"].max(), p9["total"].max())
    axis.set_ylim(0, max_total * 1.13)
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


def compose(top_path: Path, bottom_path: Path, output_path: Path) -> None:
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
    profiles: dict[tuple[int, int, int], pd.DataFrame],
    path: Path,
) -> None:
    rows = []
    for (width, height, variant), profile in sorted(profiles.items()):
        for pass_name in ("total", *VARIANTS[variant]["passes"]):
            series = profile[pass_name]
            rows.append({
                "resolution": f"{width}x{height}",
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

    with tempfile.TemporaryDirectory(prefix="visbuf_resolution_plot_") as tmp:
        root = extract_or_use(args.input, Path(tmp))
        profiles = load_profiles(root)

    resolutions = sorted(
        {(w, h) for w, h, _ in profiles},
        key=lambda x: x[0] * x[1],
    )
    if len(resolutions) != 2:
        raise ValueError(f"Expected exactly two resolutions, got {resolutions}")

    plt.style.use("seaborn-v0_8-whitegrid")

    components = []
    for width, height in resolutions:
        path = args.output_dir / f"{width}x{height}_variants_8_9_timeline.png"
        plot_resolution(profiles, (width, height), path, args.dpi)
        components.append(path)

    combined = args.output_dir / "resolution_variant_8_9_pass_timeline.png"
    compose(components[0], components[1], combined)

    summary = args.output_dir / "pass_timeline_summary.csv"
    write_summary(profiles, summary)

    manifest = {
        "layout": {
            "top": f"{resolutions[0][0]}x{resolutions[0][1]}",
            "bottom": f"{resolutions[1][0]}x{resolutions[1][1]}",
        },
        "each_panel_contains": [
            "variant 8 total and all passes",
            "variant 9 total and all passes",
        ],
        "variant_styles": {
            "8": "solid",
            "9": "dashed",
        },
        "semantic_color_pairs": {
            "depth_prepass_visibility": [
                PASS_STYLE["depth_prepass"][1],
                PASS_STYLE["visibility"][1],
            ],
            "gbuffer_make_gbuffer": [
                PASS_STYLE["geometry"][1],
                PASS_STYLE["gbuffer"][1],
            ],
        },
        "files": [p.name for p in components] + [combined.name, summary.name],
    }
    (args.output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2),
        encoding="utf-8",
    )

    print(combined)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
