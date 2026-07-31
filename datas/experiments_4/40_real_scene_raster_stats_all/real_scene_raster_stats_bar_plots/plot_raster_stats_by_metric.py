#!/usr/bin/env python3
"""
Create one bar-chart image per raster-stat metric.

The raw raster-stat CSV contains per-frame values. To keep the bar charts
readable, values are aggregated into the experiment's 60-frame profile windows.
Each chart therefore contains one bar per camera-path window.
"""

from __future__ import annotations

import argparse
import json
import re
import tempfile
import zipfile
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd
from matplotlib.ticker import FuncFormatter


METRIC_LABELS = {
    "triangle_count": "Triangle count",
    "total_fragments": "Total fragments",
    "covered_pixels": "Covered pixels",
    "overdraw_extra": "Extra fragments from overdraw",
    "avg_overdraw": "Average overdraw",
    "max_overdraw": "Maximum overdraw",
    "rasterized_triangles": "Rasterized triangles",
    "skipped_triangles": "Skipped triangles",
    "quad_instances": "Quad instances",
    "quad_covered_lanes": "Quad covered lanes",
    "quad_waste_lanes": "Quad waste lanes",
    "quad_efficiency": "Quad efficiency",
}

COUNT_METRICS = {
    "triangle_count",
    "total_fragments",
    "covered_pixels",
    "overdraw_extra",
    "max_overdraw",
    "rasterized_triangles",
    "skipped_triangles",
    "quad_instances",
    "quad_covered_lanes",
    "quad_waste_lanes",
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
    candidates = sorted(path for path in root.glob("*.csv"))
    for path in candidates:
        columns = set(pd.read_csv(path, encoding="utf-8-sig", nrows=0).columns)
        if {
            "runner_status",
            "runner_run_index",
            "scene-path",
            "profile-window-frames",
            "window-width",
            "window-height",
        }.issubset(columns):
            return path
    raise FileNotFoundError("Aggregate experiment CSV not found.")


def find_raster_stats_csv(root: Path, run_index: int) -> Path:
    runs_dirs = [
        path for path in root.iterdir()
        if path.is_dir() and path.name.endswith("_runs")
    ]
    if len(runs_dirs) != 1:
        raise ValueError(f"Expected one *_runs directory, found {runs_dirs}")

    matches = sorted(
        runs_dirs[0].glob(f"run_{run_index:05d}_*_raster_stats.csv")
    )
    if len(matches) != 1:
        raise ValueError(
            f"Expected one raster-stats CSV for run {run_index}, found {matches}"
        )
    return matches[0]


def scene_label(scene_path: str) -> str:
    stem = Path(scene_path).stem
    known = {
        "NewSponza_Main_glTF_003": "Sponza",
        "BistroExterior": "Bistro Exterior",
        "BistroInterior_Wine": "Bistro Interior Wine",
        "san-miguel": "San Miguel",
        "SunTemple": "Sun Temple",
        "MEASURE_ONE": "Zero Day Measure One",
    }
    if stem in known:
        return known[stem]
    return re.sub(r"[_-]+", " ", stem).title()


def load_data(root: Path):
    aggregate = pd.read_csv(find_aggregate(root), encoding="utf-8-sig")
    successful = aggregate.loc[aggregate["runner_status"].eq("success")].copy()
    if len(successful) != 1:
        raise ValueError(
            f"Expected exactly one successful raster-stat run, found {len(successful)}"
        )

    row = successful.iloc[0]
    run_index = int(row["runner_run_index"])
    window_frames = int(row["profile-window-frames"])
    width = int(row["window-width"])
    height = int(row["window-height"])
    scene = scene_label(str(row["scene-path"]))

    raster_path = find_raster_stats_csv(root, run_index)
    raw = pd.read_csv(raster_path, encoding="utf-8-sig")

    required = {"frame", *METRIC_LABELS}
    missing = required - set(raw.columns)
    if missing:
        raise ValueError(f"Raster-stat CSV missing columns: {sorted(missing)}")
    if raw[list(METRIC_LABELS)].isna().any().any():
        raise ValueError("Raster-stat CSV contains missing values.")

    first_frame = int(raw["frame"].min())
    raw["window_index"] = ((raw["frame"] - first_frame) // window_frames).astype(int)
    raw["window_start_frame"] = (
        first_frame + raw["window_index"] * window_frames
    )

    grouped = (
        raw.groupby(["window_index", "window_start_frame"], as_index=False)
        [list(METRIC_LABELS)]
        .mean()
    )

    return raw, grouped, {
        "scene": scene,
        "resolution": f"{width}x{height}",
        "window_frames": window_frames,
        "sample_count": len(raw),
        "window_count": len(grouped),
        "first_frame": first_frame,
        "last_frame": int(raw["frame"].max()),
    }


def compact_number(value: float, _position: int) -> str:
    absolute = abs(value)
    if absolute >= 1_000_000_000:
        return f"{value / 1_000_000_000:.1f}B"
    if absolute >= 1_000_000:
        return f"{value / 1_000_000:.1f}M"
    if absolute >= 1_000:
        return f"{value / 1_000:.1f}K"
    return f"{value:g}"


def plot_metric(
    grouped: pd.DataFrame,
    metric: str,
    metadata: dict[str, object],
    output_path: Path,
    dpi: int,
) -> None:
    figure = plt.figure(figsize=(15.5, 7.2))
    axis = figure.add_axes((0.075, 0.15, 0.90, 0.70))

    x = grouped["window_index"]
    values = grouped[metric]

    axis.bar(x, values, width=0.82)

    label = METRIC_LABELS[metric]
    mean_value = float(values.mean())
    min_value = float(values.min())
    max_value = float(values.max())

    figure.suptitle(
        f"{metadata['scene']} · {label}",
        fontsize=17,
        fontweight="bold",
        y=0.975,
    )
    figure.text(
        0.5,
        0.915,
        (
            f"{metadata['resolution']} · {metadata['sample_count']} measured frames · "
            f"{metadata['window_frames']}-frame means · "
            f"mean {mean_value:.4g} · min {min_value:.4g} · max {max_value:.4g}"
        ),
        ha="center",
        va="center",
        fontsize=10.3,
    )

    tick_step = max(1, len(grouped) // 10)
    tick_rows = grouped.iloc[::tick_step]
    axis.set_xticks(
        tick_rows["window_index"],
        tick_rows["window_start_frame"].astype(int).astype(str),
    )
    axis.set_xlabel("Window start frame")
    axis.set_ylabel(label)
    axis.grid(True, axis="y", alpha=0.25)
    axis.set_xlim(-0.7, len(grouped) - 0.3)
    axis.set_ylim(0, max_value * 1.10 if max_value > 0 else 1)

    if metric in COUNT_METRICS:
        axis.yaxis.set_major_formatter(FuncFormatter(compact_number))
    elif metric == "quad_efficiency":
        axis.yaxis.set_major_formatter(
            FuncFormatter(lambda value, _position: f"{value:.2f}")
        )
    else:
        axis.yaxis.set_major_formatter(
            FuncFormatter(lambda value, _position: f"{value:.2f}")
        )

    figure.savefig(output_path, dpi=dpi, bbox_inches="tight")
    plt.close(figure)


def main() -> int:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="raster_stats_bar_") as temp_dir:
        root = extract_or_use(args.input, Path(temp_dir))
        raw, grouped, metadata = load_data(root)

    created: list[Path] = []
    for index, metric in enumerate(METRIC_LABELS, start=1):
        output_path = args.output_dir / f"{index:02d}_{metric}_bar.png"
        plot_metric(grouped, metric, metadata, output_path, args.dpi)
        created.append(output_path)

    window_summary = grouped.copy()
    window_summary.to_csv(
        args.output_dir / "raster_stats_60_frame_window_means.csv",
        index=False,
        float_format="%.6f",
    )
    created.append(args.output_dir / "raster_stats_60_frame_window_means.csv")

    overall_rows = []
    for metric, label in METRIC_LABELS.items():
        series = raw[metric]
        overall_rows.append(
            {
                "metric": metric,
                "label": label,
                "samples": len(series),
                "mean": series.mean(),
                "median": series.median(),
                "min": series.min(),
                "max": series.max(),
                "p90": series.quantile(0.90),
                "p99": series.quantile(0.99),
            }
        )
    pd.DataFrame(overall_rows).to_csv(
        args.output_dir / "raster_stats_overall_summary.csv",
        index=False,
        float_format="%.6f",
    )
    created.append(args.output_dir / "raster_stats_overall_summary.csv")

    manifest = {
        **metadata,
        "aggregation": "arithmetic mean within each profile window",
        "metrics": list(METRIC_LABELS),
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
