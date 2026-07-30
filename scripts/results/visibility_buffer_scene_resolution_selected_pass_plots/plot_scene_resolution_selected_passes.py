#!/usr/bin/env python3
"""
Create one plot per scene x resolution combination.

Only selected passes are plotted:
- Deferred (V8, dashed): total, depth_prepass, geometry
- VisBuf  (V9, solid):  total, visibility, gbuffer

Colors:
- total: black
- prepass / visibility: red
- geometry / gbuffer: orange
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


VARIANTS = {
    8: {
        "renderer": "DonutDeferredPrepass",
        "label_prefix": "V8",
        "linestyle": "--",
        "passes": ("total", "depth_prepass", "geometry"),
    },
    9: {
        "renderer": "DonutVisGBuffer",
        "label_prefix": "V9",
        "linestyle": "-",
        "passes": ("total", "visibility", "gbuffer"),
    },
}

PASS_STYLE = {
    "total": {"color": "#111111", "linewidth": 2.8, "marker": "o"},
    "depth_prepass": {"color": "#D62728", "linewidth": 2.0, "marker": "^"},
    "visibility": {"color": "#D62728", "linewidth": 2.0, "marker": "^"},
    "geometry": {"color": "#F28E2B", "linewidth": 2.0, "marker": "s"},
    "gbuffer": {"color": "#F28E2B", "linewidth": 2.0, "marker": "s"},
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
            "scene-path",
            "window-width",
            "window-height",
        }.issubset(columns):
            return path
    raise FileNotFoundError("Aggregate experiment CSV not found.")


def scene_label_from_path(scene_path: str) -> str:
    path = Path(scene_path)
    stem = path.stem
    parent = path.parent.name

    manual = {
        "NewSponza_Main_glTF_003": "Sponza",
        "BistroExterior": "Bistro Exterior",
        "BistroInterior_Wine": "Bistro Interior Wine",
        "san-miguel": "San Miguel",
        "SunTemple": "Sun Temple",
        "MEASURE_ONE": "Zero Day Measure One",
    }
    if stem in manual:
        return manual[stem]
    if parent in manual:
        return manual[parent]
    text = stem.replace("_", " ").replace("-", " ")
    return re.sub(r"\s+", " ", text).strip().title()


def slugify(text: str) -> str:
    text = text.lower().strip()
    text = re.sub(r"[^a-z0-9]+", "_", text)
    return text.strip("_")


def load_profiles(root: Path):
    aggregate = pd.read_csv(find_aggregate(root), encoding="utf-8-sig")
    aggregate = aggregate.loc[aggregate["runner_status"].eq("success")].copy()

    runs_dirs = [
        path for path in root.iterdir()
        if path.is_dir() and path.name.endswith("_runs")
    ]
    if len(runs_dirs) != 1:
        raise ValueError(f"Expected one *_runs directory, got {runs_dirs}")
    runs_dir = runs_dirs[0]

    combo_data: dict[tuple[str, int, int], dict[int, pd.DataFrame]] = {}

    for _, row in aggregate.iterrows():
        variant = int(row["renderer-variant"])
        if variant not in VARIANTS:
            continue

        renderer = str(row["renderer_name"])
        if renderer != VARIANTS[variant]["renderer"]:
            raise ValueError(
                f"Variant {variant}: expected {VARIANTS[variant]['renderer']}, got {renderer}"
            )

        scene_label = scene_label_from_path(str(row["scene-path"]))
        width = int(row["window-width"])
        height = int(row["window-height"])
        key = (scene_label, width, height)

        run_index = int(row["runner_run_index"])
        matches = sorted(runs_dir.glob(f"run_{run_index:05d}.csv_*_result.csv"))
        if len(matches) != 1:
            raise ValueError(f"Run {run_index}: found {matches}")

        profile = pd.read_csv(matches[0], encoding="utf-8-sig")
        required = {"frame", "index_count", *VARIANTS[variant]["passes"]}
        missing = required - set(profile.columns)
        if missing:
            raise ValueError(f"{matches[0].name}: missing {sorted(missing)}")

        numeric = list(required - {"frame"})
        if profile[numeric].isna().any().any():
            raise ValueError(f"{matches[0].name}: missing numeric data")
        if (profile[numeric] < 0).any().any():
            raise ValueError(f"{matches[0].name}: negative timing data")

        combo_data.setdefault(key, {})[variant] = profile

    # validate pairs
    validated = {}
    for key, profiles in combo_data.items():
        if set(profiles) != {8, 9}:
            raise ValueError(f"{key}: missing one of variants 8/9")
        p8 = profiles[8]
        p9 = profiles[9]
        if p8["frame"].tolist() != p9["frame"].tolist():
            raise ValueError(f"{key}: frame timelines differ")
        if p8["index_count"].tolist() != p9["index_count"].tolist():
            raise ValueError(f"{key}: workload differs")
        validated[key] = profiles

    return validated


def build_summary(combo_data) -> pd.DataFrame:
    rows = []
    for (scene_label, width, height), profiles in sorted(combo_data.items()):
        for variant, config in VARIANTS.items():
            profile = profiles[variant]
            for column in config["passes"]:
                series = profile[column]
                rows.append(
                    {
                        "scene": scene_label,
                        "resolution": f"{width}x{height}",
                        "variant": variant,
                        "metric": f"{config['label_prefix']} {column}",
                        "mean_ms": series.mean(),
                        "median_ms": series.median(),
                        "p90_ms": series.quantile(0.90),
                        "p99_ms": series.quantile(0.99),
                        "min_ms": series.min(),
                        "max_ms": series.max(),
                    }
                )
    return pd.DataFrame(rows)


def plot_combo(scene_label: str, width: int, height: int, profiles, output_path: Path, dpi: int) -> None:
    p8 = profiles[8]
    p9 = profiles[9]

    figure = plt.figure(figsize=(15.8, 7.2))
    axis = figure.add_axes((0.075, 0.14, 0.90, 0.72))

    for variant, config in VARIANTS.items():
        profile = profiles[variant]
        for column in config["passes"]:
            style = PASS_STYLE[column]
            axis.plot(
                profile["frame"],
                profile[column],
                label=f"{config['label_prefix']} {column}",
                linestyle=config["linestyle"],
                color=style["color"],
                linewidth=style["linewidth"],
                marker=style["marker"],
                markersize=4.0,
                markevery=max(1, len(profile) // 18),
            )

    frame_step = int(p8["frame"].diff().dropna().mode().iloc[0])

    figure.suptitle(
        f"{scene_label} · {width}×{height}",
        fontsize=16.8,
        fontweight="bold",
        y=0.975,
    )
    figure.text(
        0.5,
        0.915,
        (
            f"selected-pass frame timeline · {len(p8)} profile windows · "
            f"{frame_step}-frame windows · Deferred dashed · VisBuf solid"
        ),
        ha="center",
        va="center",
        fontsize=10.4,
        color="#444444",
    )

    ymax = max(
        float(p8["total"].max()),
        float(p8["depth_prepass"].max()),
        float(p8["geometry"].max()),
        float(p9["total"].max()),
        float(p9["visibility"].max()),
        float(p9["gbuffer"].max()),
    ) * 1.12

    axis.set_xlim(p8["frame"].min(), p8["frame"].max())
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

    with tempfile.TemporaryDirectory(prefix="scene_resolution_selected_") as tmp:
        root = extract_or_use(args.input, Path(tmp))
        combo_data = load_profiles(root)

    created_files = []
    for (scene_label, width, height), profiles in sorted(combo_data.items()):
        filename = f"{slugify(scene_label)}_{width}x{height}_selected_pass_timeline.png"
        path = args.output_dir / filename
        plot_combo(scene_label, width, height, profiles, path, args.dpi)
        created_files.append(path)

    summary = build_summary(combo_data)
    summary_path = args.output_dir / "scene_resolution_selected_pass_summary.csv"
    summary.to_csv(summary_path, index=False, float_format="%.6f")
    created_files.append(summary_path)

    manifest = {
        "input": str(args.input.resolve()),
        "combination_count": len(combo_data),
        "figure_contents": {
            "variant_8_deferred_dashed": ["total", "depth_prepass", "geometry"],
            "variant_9_visbuf_solid": ["total", "visibility", "gbuffer"],
        },
        "colors": {
            "total": "black",
            "prepass_visibility": "red",
            "geometry_gbuffer": "orange",
        },
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
