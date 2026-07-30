#!/usr/bin/env python3
"""
Create one plot per scene from the many-scene dataset.

Each scene figure overlays all profiled passes for:
- Variant 8 (Deferred + Prepass)
- Variant 9 (Visibility + G-buffer)

Including total time for each variant.
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
        "linestyle": "-",
        "passes": ("total", "depth_prepass", "geometry", "lighting", "tonemap"),
    },
    9: {
        "renderer": "DonutVisGBuffer",
        "label_prefix": "V9",
        "linestyle": "--",
        "passes": (
            "total",
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

    scene_data: dict[str, dict[str, object]] = {}

    for _, row in aggregate.iterrows():
        variant = int(row["renderer-variant"])
        if variant not in VARIANTS:
            continue

        renderer = str(row["renderer_name"])
        if renderer != VARIANTS[variant]["renderer"]:
            raise ValueError(
                f"Variant {variant}: expected {VARIANTS[variant]['renderer']}, got {renderer}"
            )

        scene_path = str(row["scene-path"])
        scene_label = scene_label_from_path(scene_path)
        run_index = int(row["runner_run_index"])
        width = int(row["window-width"])
        height = int(row["window-height"])

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

        if scene_label not in scene_data:
            scene_data[scene_label] = {
                "scene_path": scene_path,
                "resolution": (width, height),
                "profiles": {},
            }
        else:
            if scene_data[scene_label]["resolution"] != (width, height):
                raise ValueError(f"Scene {scene_label}: inconsistent resolution")

        scene_data[scene_label]["profiles"][variant] = profile

    for scene_label, info in scene_data.items():
        profiles = info["profiles"]
        for variant in VARIANTS:
            if variant not in profiles:
                raise ValueError(f"{scene_label}: missing variant {variant}")
        p8 = profiles[8]
        p9 = profiles[9]
        if p8["frame"].tolist() != p9["frame"].tolist():
            raise ValueError(f"{scene_label}: frame timelines differ")
        if p8["index_count"].tolist() != p9["index_count"].tolist():
            raise ValueError(f"{scene_label}: workload differs")

    return scene_data


def build_summary(scene_data) -> pd.DataFrame:
    rows = []
    for scene_label, info in sorted(scene_data.items()):
        width, height = info["resolution"]
        for variant, config in VARIANTS.items():
            profile = info["profiles"][variant]
            for column in config["passes"]:
                series = profile[column]
                rows.append(
                    {
                        "scene": scene_label,
                        "resolution": f"{width}x{height}",
                        "variant": variant,
                        "metric": f"{config['label_prefix']} {column}",
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


def plot_scene(scene_label: str, info, output_path: Path, dpi: int) -> None:
    width, height = info["resolution"]
    p8 = info["profiles"][8]
    p9 = info["profiles"][9]

    figure = plt.figure(figsize=(16.2, 7.6))
    axis = figure.add_axes((0.07, 0.14, 0.91, 0.72))

    series_specs = []
    for variant, config in VARIANTS.items():
        profile = info["profiles"][variant]
        for idx, column in enumerate(config["passes"]):
            linewidth = 2.5 if column == "total" else 1.8
            marker = None
            if variant == 9 and column in {"total", "visibility", "gbuffer"}:
                marker = "o" if column == "total" else ("s" if column == "gbuffer" else "^")
            elif variant == 8 and column in {"total", "depth_prepass", "geometry"}:
                marker = "o" if column == "total" else ("s" if column == "geometry" else "^")
            series_specs.append(
                (profile, column, f"{config['label_prefix']} {column}",
                 config["linestyle"], marker, linewidth)
            )

    for profile, column, label, linestyle, marker, linewidth in series_specs:
        axis.plot(
            profile["frame"],
            profile[column],
            label=label,
            linestyle=linestyle,
            marker=marker,
            linewidth=linewidth,
            markersize=3.8 if marker is not None else None,
            markevery=max(1, len(profile) // 18) if marker is not None else None,
        )

    frame_step = int(p8["frame"].diff().dropna().mode().iloc[0])

    figure.suptitle(
        f"{scene_label} · all-pass frame timeline",
        fontsize=16.8,
        fontweight="bold",
        y=0.975,
    )
    figure.text(
        0.5,
        0.915,
        (
            f"{width}×{height} · {len(p8)} profile windows · "
            f"{frame_step}-frame windows · "
            f"V8 total mean {p8['total'].mean():.4f} ms · "
            f"V9 total mean {p9['total'].mean():.4f} ms"
        ),
        ha="center",
        va="center",
        fontsize=10.4,
        color="#444444",
    )

    ymax = max(
        max(float(p8[column].max()) for column in VARIANTS[8]["passes"]),
        max(float(p9[column].max()) for column in VARIANTS[9]["passes"]),
    ) * 1.12

    axis.set_xlim(p8["frame"].min(), p8["frame"].max())
    axis.set_ylim(0, ymax)
    axis.set_xlabel("Frame timeline")
    axis.set_ylabel("GPU time (ms)")
    axis.grid(True, alpha=0.25)
    axis.legend(loc="upper left", frameon=True, ncol=3, fontsize=9)

    figure.savefig(output_path, dpi=dpi)
    plt.close(figure)


def main() -> int:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="many_scene_plot_") as tmp:
        root = extract_or_use(args.input, Path(tmp))
        scene_data = load_profiles(root)

    plt.style.use("seaborn-v0_8-whitegrid")

    created_files = []
    for scene_label, info in sorted(scene_data.items()):
        filename = f"{slugify(scene_label)}_all_pass_timeline.png"
        path = args.output_dir / filename
        plot_scene(scene_label, info, path, args.dpi)
        created_files.append(path)

    summary = build_summary(scene_data)
    summary_path = args.output_dir / "many_scene_all_pass_summary.csv"
    summary.to_csv(summary_path, index=False, float_format="%.6f")
    created_files.append(summary_path)

    manifest = {
        "input": str(args.input.resolve()),
        "scene_count": len(scene_data),
        "scenes": sorted(scene_data.keys()),
        "figure_contents": {
            "variant_8": list(VARIANTS[8]["passes"]),
            "variant_9": list(VARIANTS[9]["passes"]),
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
