#!/usr/bin/env python3
"""
Create one figure per resolution from the resolution-halving dataset.

Each figure contains 6 time-series across the frame timeline:
    - V8 Total
    - V8 G-buffer make (geometry)
    - V8 Depth prepass
    - V9 Total
    - V9 G-buffer
    - V9 Visibility
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
        "required_columns": ("total", "geometry", "depth_prepass"),
    },
    9: {
        "renderer": "DonutVisGBuffer",
        "required_columns": ("total", "gbuffer", "visibility"),
    },
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

    runs_dirs = [
        path for path in root.iterdir()
        if path.is_dir() and path.name.endswith("_runs")
    ]
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
                f"Variant {variant}: expected {VARIANTS[variant]['renderer']}, got {renderer}"
            )

        width = int(row["window-width"])
        height = int(row["window-height"])
        run_index = int(row["runner_run_index"])

        matches = sorted(runs_dir.glob(f"run_{run_index:05d}.csv_*_result.csv"))
        if len(matches) != 1:
            raise ValueError(f"Run {run_index}: found {matches}")

        profile = pd.read_csv(matches[0], encoding="utf-8-sig")
        required = {"frame", "index_count", *VARIANTS[variant]["required_columns"]}
        missing = required - set(profile.columns)
        if missing:
            raise ValueError(f"{matches[0].name}: missing {sorted(missing)}")

        numeric = list(required - {"frame"})
        if profile[numeric].isna().any().any():
            raise ValueError(f"{matches[0].name}: missing numeric data")
        if (profile[numeric] < 0).any().any():
            raise ValueError(f"{matches[0].name}: negative timing data")

        key = (width, height, variant)
        if key in profiles:
            raise ValueError(f"Duplicate profile for {key}")
        profiles[key] = profile

    resolutions = sorted(
        {(w, h) for w, h, _ in profiles},
        key=lambda x: x[0] * x[1]
    )

    for width, height in resolutions:
        for variant in VARIANTS:
            if (width, height, variant) not in profiles:
                raise ValueError(f"Missing variant {variant} at {width}x{height}")

        p8 = profiles[(width, height, 8)]
        p9 = profiles[(width, height, 9)]
        if p8["frame"].tolist() != p9["frame"].tolist():
            raise ValueError(f"Frame timelines differ at {width}x{height}")
        if p8["index_count"].tolist() != p9["index_count"].tolist():
            raise ValueError(f"Workload differs at {width}x{height}")

    return profiles


def build_summary(profiles: dict[tuple[int, int, int], pd.DataFrame]) -> pd.DataFrame:
    rows = []
    metric_map = {
        8: [
            ("V8 Total", "total"),
            ("V8 G-buffer make", "geometry"),
            ("V8 Depth prepass", "depth_prepass"),
        ],
        9: [
            ("V9 Total", "total"),
            ("V9 G-buffer", "gbuffer"),
            ("V9 Visibility", "visibility"),
        ],
    }

    for (width, height, variant), profile in sorted(profiles.items()):
        for metric_name, column in metric_map[variant]:
            series = profile[column]
            rows.append(
                {
                    "resolution": f"{width}x{height}",
                    "width": width,
                    "height": height,
                    "variant": variant,
                    "metric": metric_name,
                    "column_name": column,
                    "mean_ms": series.mean(),
                    "median_ms": series.median(),
                    "p90_ms": series.quantile(0.90),
                    "p99_ms": series.quantile(0.99),
                    "min_ms": series.min(),
                    "max_ms": series.max(),
                }
            )
    return pd.DataFrame(rows)


def plot_one_resolution(
    profiles: dict[tuple[int, int, int], pd.DataFrame],
    resolution: tuple[int, int],
    output_path: Path,
    dpi: int,
) -> None:
    width, height = resolution
    p8 = profiles[(width, height, 8)]
    p9 = profiles[(width, height, 9)]

    figure = plt.figure(figsize=(15.5, 7.2))
    axis = figure.add_axes((0.075, 0.14, 0.90, 0.72))

    series_specs = [
        (p8, "total", "V8 Total", "-", None, 2.5),
        (p8, "geometry", "V8 G-buffer make", "--", None, 2.0),
        (p8, "depth_prepass", "V8 Depth prepass", ":", None, 2.0),
        (p9, "total", "V9 Total", "-", "o", 2.5),
        (p9, "gbuffer", "V9 G-buffer", "--", "s", 2.0),
        (p9, "visibility", "V9 Visibility", ":", "^", 2.0),
    ]

    for profile, column, label, linestyle, marker, linewidth in series_specs:
        axis.plot(
            profile["frame"],
            profile[column],
            label=label,
            linestyle=linestyle,
            marker=marker,
            linewidth=linewidth,
            markersize=4.0 if marker is not None else None,
            markevery=max(1, len(profile) // 16) if marker is not None else None,
        )

    frame_step = int(p8["frame"].diff().dropna().mode().iloc[0])

    figure.suptitle(
        f"{width}×{height} · Variant 8 / 9 · Total, G-buffer, Prepass, Visibility timeline",
        fontsize=16.5,
        fontweight="bold",
        y=0.975,
    )
    figure.text(
        0.5,
        0.915,
        (
            f"{len(p8)} profile windows · {frame_step}-frame windows · "
            f"V8 total mean {p8['total'].mean():.4f} ms · "
            f"V9 total mean {p9['total'].mean():.4f} ms"
        ),
        ha="center",
        va="center",
        fontsize=10.3,
        color="#444444",
    )

    axis.set_xlim(p8["frame"].min(), p8["frame"].max())
    ymax = max(
        p8["total"].max(),
        p8["geometry"].max(),
        p8["depth_prepass"].max(),
        p9["total"].max(),
        p9["gbuffer"].max(),
        p9["visibility"].max(),
    ) * 1.13
    axis.set_ylim(0, ymax)
    axis.set_xlabel("Frame timeline")
    axis.set_ylabel("GPU time (ms)")
    axis.grid(True, alpha=0.25)
    axis.legend(loc="upper left", frameon=True, ncol=2, fontsize=10)

    figure.savefig(output_path, dpi=dpi)
    plt.close(figure)


def main() -> int:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="resolution_halving_sixline_plot_") as tmp:
        root = extract_or_use(args.input, Path(tmp))
        profiles = load_profiles(root)

    resolutions = sorted(
        {(w, h) for w, h, _ in profiles},
        key=lambda x: x[0] * x[1]
    )

    created_files = []
    for width, height in resolutions:
        path = args.output_dir / f"{width}x{height}_variant_8_9_total_gbuffer_prepass_visibility_timeline.png"
        plot_one_resolution(profiles, (width, height), path, args.dpi)
        created_files.append(path)

    summary = build_summary(profiles)
    summary_path = args.output_dir / "resolution_halving_sixline_summary.csv"
    summary.to_csv(summary_path, index=False, float_format="%.6f")
    created_files.append(summary_path)

    manifest = {
        "input": str(args.input.resolve()),
        "resolutions": [f"{w}x{h}" for w, h in resolutions],
        "figure_contents": [
            "V8 Total",
            "V8 G-buffer make",
            "V8 Depth prepass",
            "V9 Total",
            "V9 G-buffer",
            "V9 Visibility",
        ],
        "created_files": [path.name for path in created_files],
    }
    manifest_path = args.output_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    created_files.append(manifest_path)

    print(f"Created {len(created_files)} files in {args.output_dir.resolve()}")
    for path in created_files:
        print(path.name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
