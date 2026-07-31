#!/usr/bin/env python3
"""
Plot renderer variants 8 and 9 as pass-by-pass frame timelines.

For each resolution, the script creates:
  - one Variant 8 component chart
  - one Variant 9 component chart
  - one vertically composed comparison image (Variant 8 above Variant 9)

Usage:
    python plot_visibility_buffer_timeline.py \
        --input 33_bistro_pbr_resolution_renderer_compare.zip \
        --output-dir plots

The input may be either the experiment ZIP or its extracted result directory.
"""

from __future__ import annotations

import argparse
import csv
import json
import shutil
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
        "title": "Variant 8 — Deferred + Depth Prepass",
        "expected_renderer": "DonutDeferredPrepass",
        "passes": ("depth_prepass", "geometry", "lighting", "tonemap"),
    },
    9: {
        "title": "Variant 9 — Visibility Buffer + G-buffer",
        "expected_renderer": "DonutVisGBuffer",
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

# Semantic color pairing:
#   depth_prepass <-> visibility: blue/teal family
#   geometry <-> gbuffer: green family
# Common passes keep exactly the same color across variants.
PASS_STYLE = {
    "total": {
        "label": "Total",
        "color": "#202124",
        "linewidth": 2.5,
        "linestyle": "-",
        "zorder": 20,
    },
    "depth_prepass": {
        "label": "Depth prepass",
        "color": "#4C78A8",
        "linewidth": 1.65,
        "linestyle": "-",
        "zorder": 8,
    },
    "visibility": {
        "label": "Visibility",
        "color": "#72B7B2",
        "linewidth": 1.65,
        "linestyle": "--",
        "zorder": 8,
    },
    "geometry": {
        "label": "G-buffer make (geometry)",
        "color": "#59A14F",
        "linewidth": 1.65,
        "linestyle": "-",
        "zorder": 7,
    },
    "gbuffer": {
        "label": "G-buffer",
        "color": "#8CD17D",
        "linewidth": 1.65,
        "linestyle": "--",
        "zorder": 7,
    },
    "lighting": {
        "label": "Lighting",
        "color": "#F28E2B",
        "linewidth": 1.55,
        "linestyle": "-",
        "zorder": 6,
    },
    "tonemap": {
        "label": "Tonemap",
        "color": "#B279A2",
        "linewidth": 1.55,
        "linestyle": "-",
        "zorder": 6,
    },
    "visutil_histogram": {
        "label": "Visutil histogram",
        "color": "#E15759",
        "linewidth": 1.35,
        "linestyle": "-",
        "zorder": 5,
    },
    "visutil_prefix": {
        "label": "Visutil prefix",
        "color": "#FF9D9A",
        "linewidth": 1.35,
        "linestyle": "--",
        "zorder": 5,
    },
    "visutil_flatten": {
        "label": "Visutil flatten",
        "color": "#D37295",
        "linewidth": 1.35,
        "linestyle": "-.",
        "zorder": 5,
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input",
        type=Path,
        required=True,
        help="Experiment ZIP or extracted result directory.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("visibility_buffer_timeline_plots"),
    )
    parser.add_argument("--dpi", type=int, default=220)
    return parser.parse_args()


def locate_result_root(input_path: Path, temp_root: Path) -> Path:
    input_path = input_path.resolve()
    if input_path.is_dir():
        return input_path
    if not input_path.is_file() or input_path.suffix.lower() != ".zip":
        raise ValueError(f"Expected a ZIP or directory: {input_path}")

    extracted = temp_root / input_path.stem
    extracted.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(input_path) as archive:
        archive.extractall(extracted)
    return extracted


def find_aggregate_csv(result_root: Path) -> Path:
    candidates = sorted(
        path
        for path in result_root.glob("*.csv")
        if "_run" not in path.stem and not path.stem.startswith("run_")
    )
    for path in candidates:
        columns = pd.read_csv(path, encoding="utf-8-sig", nrows=0).columns
        if {"runner_status", "runner_run_index", "renderer-variant"}.issubset(columns):
            return path
    raise FileNotFoundError("Could not find the aggregate experiment CSV.")


def load_runs(result_root: Path) -> tuple[pd.DataFrame, dict[tuple[int, int, int], pd.DataFrame]]:
    aggregate_path = find_aggregate_csv(result_root)
    aggregate = pd.read_csv(aggregate_path, encoding="utf-8-sig")
    aggregate = aggregate.loc[aggregate["runner_status"].eq("success")].copy()

    required = {
        "runner_run_index",
        "renderer-variant",
        "window-width",
        "window-height",
        "renderer_name",
    }
    missing = required - set(aggregate.columns)
    if missing:
        raise ValueError(f"Aggregate CSV is missing fields: {sorted(missing)}")

    runs_dirs = [path for path in result_root.iterdir() if path.is_dir() and path.name.endswith("_runs")]
    if len(runs_dirs) != 1:
        raise ValueError(f"Expected one *_runs directory, found: {runs_dirs}")
    runs_dir = runs_dirs[0]

    profiles: dict[tuple[int, int, int], pd.DataFrame] = {}
    for _, row in aggregate.iterrows():
        variant = int(row["renderer-variant"])
        if variant not in VARIANTS:
            continue

        width = int(row["window-width"])
        height = int(row["window-height"])
        run_index = int(row["runner_run_index"])
        renderer_name = str(row["renderer_name"])

        expected_renderer = VARIANTS[variant]["expected_renderer"]
        if renderer_name != expected_renderer:
            raise ValueError(
                f"Variant {variant}: renderer_name={renderer_name}, "
                f"expected {expected_renderer}"
            )

        matches = sorted(runs_dir.glob(f"run_{run_index:05d}.csv_*_result.csv"))
        if len(matches) != 1:
            raise ValueError(
                f"Run {run_index}: expected one profile result, found {matches}"
            )

        profile = pd.read_csv(matches[0], encoding="utf-8-sig")
        required_columns = {
            "frame",
            "total",
            "index_count",
            *VARIANTS[variant]["passes"],
        }
        missing_columns = required_columns - set(profile.columns)
        if missing_columns:
            raise ValueError(
                f"{matches[0].name} missing columns: {sorted(missing_columns)}"
            )

        if profile[list(required_columns - {"frame"})].isna().any().any():
            raise ValueError(f"{matches[0].name} contains missing numeric values.")
        if (profile[list(required_columns - {"frame"})] < 0).any().any():
            raise ValueError(f"{matches[0].name} contains negative values.")

        key = (width, height, variant)
        if key in profiles:
            raise ValueError(f"Duplicate profile for {key}")
        profiles[key] = profile

    resolutions = sorted({(w, h) for w, h, _ in profiles})
    for width, height in resolutions:
        for variant in VARIANTS:
            if (width, height, variant) not in profiles:
                raise ValueError(
                    f"Missing variant {variant} at {width}x{height}"
                )

        reference = profiles[(width, height, 8)]
        compared = profiles[(width, height, 9)]
        if reference["frame"].tolist() != compared["frame"].tolist():
            raise ValueError(f"Frame timeline differs at {width}x{height}")
        if reference["index_count"].tolist() != compared["index_count"].tolist():
            raise ValueError(f"Index-count workload differs at {width}x{height}")

    return aggregate, profiles


def draw_variant_chart(
    profile: pd.DataFrame,
    variant: int,
    resolution: tuple[int, int],
    output_path: Path,
    y_limit: float,
    dpi: int,
) -> None:
    width, height = resolution
    config = VARIANTS[variant]

    # One chart per figure. The final top/bottom layout is composed afterward.
    figure = plt.figure(figsize=(15.5, 6.2))
    axis = figure.add_axes((0.075, 0.14, 0.90, 0.72))

    columns = ("total", *config["passes"])
    for column in columns:
        style = PASS_STYLE[column]
        axis.plot(
            profile["frame"],
            profile[column],
            label=style["label"],
            color=style["color"],
            linewidth=style["linewidth"],
            linestyle=style["linestyle"],
            zorder=style["zorder"],
        )

    total_mean = profile["total"].mean()
    total_median = profile["total"].median()
    profile_step = int(profile["frame"].diff().dropna().mode().iloc[0])

    figure.suptitle(
        f"{config['title']}  |  {width}×{height}",
        fontsize=16.5,
        fontweight="bold",
        y=0.97,
    )
    figure.text(
        0.5,
        0.905,
        (
            f"Camera-path frame timeline · {len(profile)} profile windows · "
            f"{profile_step}-frame window · total mean {total_mean:.4f} ms · "
            f"median {total_median:.4f} ms"
        ),
        ha="center",
        va="center",
        fontsize=10.2,
        color="#444444",
    )

    axis.set_xlim(profile["frame"].min(), profile["frame"].max())
    axis.set_ylim(0, y_limit)
    axis.set_xlabel("Frame timeline")
    axis.set_ylabel("GPU time (ms)")
    axis.grid(True, alpha=0.25, linewidth=0.8)
    axis.legend(
        loc="upper left",
        ncol=4,
        frameon=True,
        fontsize=9.2,
        columnspacing=1.2,
        handlelength=2.7,
    )

    figure.savefig(output_path, dpi=dpi)
    plt.close(figure)


def compose_vertical(top_path: Path, bottom_path: Path, output_path: Path) -> None:
    with Image.open(top_path) as top_image, Image.open(bottom_path) as bottom_image:
        top = top_image.convert("RGB")
        bottom = bottom_image.convert("RGB")

        target_width = max(top.width, bottom.width)
        if top.width != target_width:
            scaled_height = round(top.height * target_width / top.width)
            top = top.resize((target_width, scaled_height), Image.Resampling.LANCZOS)
        if bottom.width != target_width:
            scaled_height = round(bottom.height * target_width / bottom.width)
            bottom = bottom.resize((target_width, scaled_height), Image.Resampling.LANCZOS)

        gap = 18
        canvas = Image.new(
            "RGB",
            (target_width, top.height + gap + bottom.height),
            "white",
        )
        canvas.paste(top, (0, 0))
        canvas.paste(bottom, (0, top.height + gap))
        canvas.save(output_path, quality=96)


def write_summary(
    output_path: Path,
    profiles: dict[tuple[int, int, int], pd.DataFrame],
) -> None:
    rows: list[dict[str, object]] = []
    for (width, height, variant), profile in sorted(profiles.items()):
        for column in ("total", *VARIANTS[variant]["passes"]):
            series = profile[column]
            rows.append(
                {
                    "resolution": f"{width}x{height}",
                    "variant": variant,
                    "renderer": VARIANTS[variant]["expected_renderer"],
                    "pass": column,
                    "mean_ms": series.mean(),
                    "median_ms": series.median(),
                    "min_ms": series.min(),
                    "max_ms": series.max(),
                    "p90_ms": series.quantile(0.90),
                    "p99_ms": series.quantile(0.99),
                }
            )
    pd.DataFrame(rows).to_csv(output_path, index=False, float_format="%.6f")


def main() -> int:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="visbuf_plot_") as temp_dir:
        result_root = locate_result_root(args.input, Path(temp_dir))
        aggregate, profiles = load_runs(result_root)

    plt.style.use("seaborn-v0_8-whitegrid")

    created: list[Path] = []
    resolutions = sorted({(w, h) for w, h, _ in profiles})
    for width, height in resolutions:
        resolution_profiles = {
            variant: profiles[(width, height, variant)]
            for variant in VARIANTS
        }
        shared_y_limit = (
            max(profile["total"].max() for profile in resolution_profiles.values())
            * 1.10
        )

        component_paths: dict[int, Path] = {}
        for variant, profile in resolution_profiles.items():
            path = args.output_dir / (
                f"{width}x{height}_variant_{variant}_pass_timeline.png"
            )
            draw_variant_chart(
                profile,
                variant,
                (width, height),
                path,
                shared_y_limit,
                args.dpi,
            )
            component_paths[variant] = path
            created.append(path)

        combined_path = args.output_dir / (
            f"{width}x{height}_variant_8_9_pass_timeline.png"
        )
        compose_vertical(
            component_paths[8],
            component_paths[9],
            combined_path,
        )
        created.append(combined_path)

    summary_path = args.output_dir / "pass_timeline_summary.csv"
    write_summary(summary_path, profiles)
    created.append(summary_path)

    manifest = {
        "input": str(args.input.resolve()),
        "resolutions": [f"{w}x{h}" for w, h in resolutions],
        "variants": sorted(VARIANTS),
        "frame_windows": {
            f"{w}x{h}": len(profiles[(w, h, 8)])
            for w, h in resolutions
        },
        "color_pairing": {
            "depth_prepass_visibility": [
                PASS_STYLE["depth_prepass"]["color"],
                PASS_STYLE["visibility"]["color"],
            ],
            "geometry_gbuffer": [
                PASS_STYLE["geometry"]["color"],
                PASS_STYLE["gbuffer"]["color"],
            ],
            "lighting": PASS_STYLE["lighting"]["color"],
            "tonemap": PASS_STYLE["tonemap"]["color"],
            "total": PASS_STYLE["total"]["color"],
        },
        "created_files": [path.name for path in created],
    }
    manifest_path = args.output_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    created.append(manifest_path)

    print(f"Created {len(created)} files in {args.output_dir.resolve()}")
    for path in created:
        print(path.name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
