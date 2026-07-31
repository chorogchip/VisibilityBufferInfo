#!/usr/bin/env python3
"""Compare raster-reference and analytic VisBuf debug captures frame by frame."""

from __future__ import annotations

import argparse
import csv
import json
import math
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image, ImageDraw, ImageFont


SCRIPT_DIR = Path(__file__).resolve().parent
RESULTS_DIR = SCRIPT_DIR / "results"
ANALYSIS_DIR = SCRIPT_DIR / "analysis"
DATA_DIR = ANALYSIS_DIR / "data"
PNG_DIR = ANALYSIS_DIR / "png"
SVG_DIR = ANALYSIS_DIR / "svg"
REPRESENTATIVE_DIR = ANALYSIS_DIR / "representative"
LOCAL_HEATMAP_DIR = ANALYSIS_DIR / "local_heatmaps"

GPU_NAME = "NVIDIA GeForce RTX 5060 Ti 16GB"
BACKGROUND_RGB = np.array([26, 26, 38], dtype=np.int16)
BACKGROUND_TOLERANCE_LSB = 1
INTERIOR_EROSION_PIXELS = 1

MODE_NAMES = {
    3: "linear_barycentric",
    4: "perspective_barycentric",
    5: "linear_barycentric_dx",
    6: "linear_barycentric_dy",
    7: "uv_dx",
    8: "uv_dy",
    9: "uv_lod_proxy_1024px",
}
MODE_LABELS = {
    3: "Linear barycentric",
    4: "Perspective barycentric",
    5: "Linear barycentric dx",
    6: "Linear barycentric dy",
    7: "UV dx",
    8: "UV dy",
    9: "LOD proxy (1024 px)",
}
SCENE_NAMES = {
    "01_sponza_barycentric_validation": "Sponza",
    "02_sponza_ivy_barycentric_validation": "Sponza Ivy",
    "03_bistro_barycentric_validation": "Bistro",
}
SCENE_ORDER = ["Sponza", "Sponza Ivy", "Bistro"]


@dataclass(frozen=True)
class CaptureRun:
    scene: str
    campaign: str
    renderer_variant: int
    mode: int
    stride: int
    frames_dir: Path


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig") as file:
        value = json.load(file)
    if not isinstance(value, dict):
        raise ValueError(f"Expected JSON object: {path}")
    return value


def portable_path(path: Path) -> str:
    resolved = path.resolve()
    try:
        return resolved.relative_to(SCRIPT_DIR).as_posix()
    except ValueError:
        return resolved.name


def artifact_frames_dir(run: dict[str, Any]) -> Path:
    artifact_dirs = run.get("artifact_dirs", [])
    if not artifact_dirs:
        raise ValueError(
            f"Run {run.get('run_index')} has no preserved artifact directory."
        )
    frames_dir = Path(artifact_dirs[0]) / "frames"
    if not frames_dir.is_dir():
        raise FileNotFoundError(f"Preserved frames directory is missing: {frames_dir}")
    return frames_dir


def load_capture_runs() -> list[CaptureRun]:
    captures: list[CaptureRun] = []
    for campaign, scene in SCENE_NAMES.items():
        root = RESULTS_DIR / campaign
        report_path = root / f"{campaign}_run_report.json"
        report = read_json(report_path)
        if (
            report.get("status") != "completed"
            or report.get("successful_runs") != 14
            or report.get("failed_runs")
            or report.get("salvaged_runs")
            or report.get("skipped_runs")
        ):
            raise ValueError(f"Campaign is not a clean 14/14 completion: {report_path}")
        for run in report.get("runs", []):
            parameters = run["parameters"]
            captures.append(
                CaptureRun(
                    scene=scene,
                    campaign=campaign,
                    renderer_variant=int(parameters["renderer_variant"]),
                    mode=int(parameters["visibility_debug_mode"]),
                    stride=int(parameters["capture_stride"]),
                    frames_dir=artifact_frames_dir(run),
                )
            )
    return captures


def foreground_mask(image: np.ndarray) -> np.ndarray:
    delta = np.abs(image.astype(np.int16) - BACKGROUND_RGB)
    return np.any(delta > BACKGROUND_TOLERANCE_LSB, axis=2)


def erode_mask(mask: np.ndarray, pixels: int = 1) -> np.ndarray:
    result = mask.copy()
    for _ in range(pixels):
        source = result
        eroded = source.copy()
        eroded[1:, :] &= source[:-1, :]
        eroded[:-1, :] &= source[1:, :]
        eroded[:, 1:] &= source[:, :-1]
        eroded[:, :-1] &= source[:, 1:]
        eroded[1:, 1:] &= source[:-1, :-1]
        eroded[1:, :-1] &= source[:-1, 1:]
        eroded[:-1, 1:] &= source[1:, :-1]
        eroded[:-1, :-1] &= source[1:, 1:]
        result = eroded
    return result


def masked_values(values: np.ndarray, mask: np.ndarray) -> np.ndarray:
    selected = values[mask]
    if selected.size == 0:
        return np.zeros(1, dtype=np.float32)
    return selected.astype(np.float32, copy=False)


def error_stats(values: np.ndarray) -> dict[str, float]:
    values = values.astype(np.float32, copy=False)
    return {
        "mae_lsb": float(np.mean(values)),
        "rmse_lsb": float(np.sqrt(np.mean(np.square(values)))),
        "p90_lsb": float(np.percentile(values, 90)),
        "p99_lsb": float(np.percentile(values, 99)),
        "max_lsb": float(np.max(values)),
        "exact_ratio": float(np.mean(values == 0)),
        "over_1_lsb_ratio": float(np.mean(values > 1)),
        "over_2_lsb_ratio": float(np.mean(values > 2)),
        "over_5_lsb_ratio": float(np.mean(values > 5)),
    }


def decoded_mae(mode: int, mae_lsb: float) -> tuple[str, float]:
    if mode in (5, 6, 7, 8):
        return "derivative_units", mae_lsb / (255.0 * 16.0)
    if mode == 9:
        return "lod_levels", mae_lsb * 24.0 / 255.0
    return "barycentric_units", mae_lsb / 255.0


def compare_frame(
    scene: str,
    mode: int,
    stride: int,
    frame_index: int,
    raster_path: Path,
    visbuf_path: Path,
) -> tuple[dict[str, Any], np.ndarray, np.ndarray, np.ndarray]:
    raster = np.asarray(Image.open(raster_path).convert("RGB"), dtype=np.uint8)
    visbuf = np.asarray(Image.open(visbuf_path).convert("RGB"), dtype=np.uint8)
    if raster.shape != visbuf.shape:
        raise ValueError(
            f"Image shape mismatch: {raster_path} {raster.shape} != "
            f"{visbuf_path} {visbuf.shape}"
        )

    difference = np.abs(
        raster.astype(np.int16) - visbuf.astype(np.int16)
    ).astype(np.uint8)
    raster_fg = foreground_mask(raster)
    visbuf_fg = foreground_mask(visbuf)
    common = raster_fg & visbuf_fg
    interior = erode_mask(common, INTERIOR_EROSION_PIXELS)
    union = raster_fg | visbuf_fg
    mismatch = raster_fg ^ visbuf_fg

    full_stats = error_stats(difference.reshape(-1))
    common_stats = error_stats(masked_values(difference, common))
    interior_stats = error_stats(masked_values(difference, interior))
    decoded_unit, decoded_value = decoded_mae(mode, interior_stats["mae_lsb"])

    row: dict[str, Any] = {
        "gpu": GPU_NAME,
        "scene": scene,
        "mode": mode,
        "mode_name": MODE_NAMES[mode],
        "capture_index": frame_index,
        "measurement_frame": frame_index * stride,
        "capture_stride": stride,
        "width": raster.shape[1],
        "height": raster.shape[0],
        "raster_path": portable_path(raster_path),
        "visbuf_path": portable_path(visbuf_path),
        "foreground_pixels_raster": int(np.count_nonzero(raster_fg)),
        "foreground_pixels_visbuf": int(np.count_nonzero(visbuf_fg)),
        "common_pixels": int(np.count_nonzero(common)),
        "interior_pixels": int(np.count_nonzero(interior)),
        "coverage_mismatch_pixels": int(np.count_nonzero(mismatch)),
        "coverage_mismatch_ratio": float(np.mean(mismatch)),
        "coverage_iou": (
            float(np.count_nonzero(common) / np.count_nonzero(union))
            if np.any(union)
            else 1.0
        ),
        "decoded_error_unit": decoded_unit,
        "interior_mae_decoded": decoded_value,
    }
    for prefix, stats in (
        ("full", full_stats),
        ("common", common_stats),
        ("interior", interior_stats),
    ):
        row.update({f"{prefix}_{key}": value for key, value in stats.items()})
    return row, raster, visbuf, difference


def write_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not rows:
        path.write_text("", encoding="utf-8")
        return
    fields: list[str] = []
    for row in rows:
        for key in row:
            if key not in fields:
                fields.append(key)
    with path.open("w", encoding="utf-8-sig", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def summarize_pairs(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, int], list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[(row["scene"], int(row["mode"]))].append(row)

    summaries: list[dict[str, Any]] = []
    for scene in SCENE_ORDER:
        for mode in MODE_NAMES:
            group = grouped[(scene, mode)]
            interior_mae = np.array(
                [float(row["interior_mae_lsb"]) for row in group]
            )
            interior_p99 = np.array(
                [float(row["interior_p99_lsb"]) for row in group]
            )
            mismatch = np.array(
                [float(row["coverage_mismatch_ratio"]) for row in group]
            )
            worst = max(group, key=lambda row: float(row["interior_mae_lsb"]))
            unit, decoded = decoded_mae(mode, float(np.mean(interior_mae)))
            summaries.append(
                {
                    "gpu": GPU_NAME,
                    "scene": scene,
                    "mode": mode,
                    "mode_name": MODE_NAMES[mode],
                    "frame_count": len(group),
                    "mean_interior_mae_lsb": float(np.mean(interior_mae)),
                    "median_interior_mae_lsb": float(np.median(interior_mae)),
                    "p90_frame_interior_mae_lsb": float(
                        np.percentile(interior_mae, 90)
                    ),
                    "max_frame_interior_mae_lsb": float(np.max(interior_mae)),
                    "mean_interior_p99_lsb": float(np.mean(interior_p99)),
                    "max_interior_p99_lsb": float(np.max(interior_p99)),
                    "mean_coverage_mismatch_ratio": float(np.mean(mismatch)),
                    "max_coverage_mismatch_ratio": float(np.max(mismatch)),
                    "mean_interior_exact_ratio": float(
                        np.mean(
                            [
                                float(row["interior_exact_ratio"])
                                for row in group
                            ]
                        )
                    ),
                    "mean_interior_over_1_lsb_ratio": float(
                        np.mean(
                            [
                                float(row["interior_over_1_lsb_ratio"])
                                for row in group
                            ]
                        )
                    ),
                    "decoded_error_unit": unit,
                    "mean_interior_mae_decoded": decoded,
                    "worst_capture_index": int(worst["capture_index"]),
                    "worst_measurement_frame": int(worst["measurement_frame"]),
                    "worst_raster_path": worst["raster_path"],
                    "worst_visbuf_path": worst["visbuf_path"],
                }
            )
    return summaries


def heatmap_image(difference: np.ndarray) -> Image.Image:
    maximum = np.max(difference.astype(np.float32), axis=2)
    normalized = np.clip(maximum / 8.0, 0.0, 1.0)
    rgba = plt.get_cmap("magma")(normalized, bytes=True)
    return Image.fromarray(rgba[:, :, :3], mode="RGB")


def font(size: int) -> ImageFont.ImageFont:
    candidates = [
        Path("C:/Windows/Fonts/segoeui.ttf"),
        Path("C:/Windows/Fonts/arial.ttf"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return ImageFont.truetype(str(candidate), size)
    return ImageFont.load_default()


def panel_with_label(image: Image.Image, label: str, width: int = 640) -> Image.Image:
    ratio = width / image.width
    height = max(1, round(image.height * ratio))
    resized = image.resize((width, height), Image.Resampling.LANCZOS)
    label_height = 40
    panel = Image.new("RGB", (width, height + label_height), "white")
    panel.paste(resized, (0, label_height))
    draw = ImageDraw.Draw(panel)
    draw.text((12, 8), label, fill=(20, 26, 36), font=font(20))
    return panel


def save_comparison(
    output_path: Path,
    raster: np.ndarray,
    visbuf: np.ndarray,
    difference: np.ndarray,
    title: str,
) -> None:
    panels = [
        panel_with_label(Image.fromarray(raster), "Raster reference"),
        panel_with_label(Image.fromarray(visbuf), "VisBuf analytic"),
        panel_with_label(heatmap_image(difference), "Absolute diff (8 LSB = white)"),
    ]
    title_height = 56
    canvas = Image.new(
        "RGB",
        (sum(panel.width for panel in panels), panels[0].height + title_height),
        "white",
    )
    draw = ImageDraw.Draw(canvas)
    draw.text((16, 14), title, fill=(10, 16, 28), font=font(24))
    x = 0
    for panel in panels:
        canvas.paste(panel, (x, title_height))
        x += panel.width
    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(output_path, optimize=True)


def save_all_frame_heatmap(
    scene: str,
    mode: int,
    capture_index: int,
    difference: np.ndarray,
) -> None:
    image = heatmap_image(difference)
    image.thumbnail((960, 540), Image.Resampling.LANCZOS)
    destination = (
        LOCAL_HEATMAP_DIR
        / scene.lower().replace(" ", "_")
        / MODE_NAMES[mode]
        / f"frame_{capture_index:06d}.webp"
    )
    destination.parent.mkdir(parents=True, exist_ok=True)
    image.save(destination, "WEBP", quality=85, method=6)


def save_plot(fig: plt.Figure, stem: str) -> None:
    PNG_DIR.mkdir(parents=True, exist_ok=True)
    SVG_DIR.mkdir(parents=True, exist_ok=True)
    fig.savefig(PNG_DIR / f"{stem}.png", dpi=180, bbox_inches="tight")
    fig.savefig(SVG_DIR / f"{stem}.svg", bbox_inches="tight")
    plt.close(fig)


def plot_summary(
    frame_rows: list[dict[str, Any]],
    pair_rows: list[dict[str, Any]],
) -> None:
    mode_order = list(MODE_NAMES)
    matrix = np.zeros((len(SCENE_ORDER), len(mode_order)))
    coverage = np.zeros_like(matrix)
    over_one = np.zeros_like(matrix)
    for row in pair_rows:
        y = SCENE_ORDER.index(str(row["scene"]))
        x = mode_order.index(int(row["mode"]))
        matrix[y, x] = float(row["mean_interior_mae_lsb"])
        coverage[y, x] = float(row["mean_coverage_mismatch_ratio"]) * 100.0
        over_one[y, x] = float(row["mean_interior_over_1_lsb_ratio"]) * 100.0

    for values, stem, title, colorbar in (
        (
            matrix,
            "aggregate_interior_mae_heatmap",
            "Raster vs analytic VisBuf: mean interior error",
            "Mean absolute error (8-bit LSB)",
        ),
        (
            coverage,
            "aggregate_coverage_mismatch_heatmap",
            "Raster vs analytic VisBuf: coverage mismatch",
            "Mismatched pixels (%)",
        ),
        (
            over_one,
            "aggregate_over_1_lsb_heatmap",
            "Raster vs analytic VisBuf: interior error above 1 LSB",
            "Interior channels above 1 LSB (%)",
        ),
    ):
        fig, ax = plt.subplots(figsize=(12.5, 4.5))
        image = ax.imshow(values, cmap="magma", aspect="auto")
        ax.set_xticks(range(len(mode_order)))
        ax.set_xticklabels(
            [MODE_LABELS[mode] for mode in mode_order], rotation=28, ha="right"
        )
        ax.set_yticks(range(len(SCENE_ORDER)))
        ax.set_yticklabels(SCENE_ORDER)
        for y in range(values.shape[0]):
            for x in range(values.shape[1]):
                rgba = image.cmap(image.norm(values[y, x]))
                luminance = (
                    0.2126 * rgba[0] + 0.7152 * rgba[1] + 0.0722 * rgba[2]
                )
                ax.text(
                    x,
                    y,
                    f"{values[y, x]:.4g}",
                    ha="center",
                    va="center",
                    color="black" if luminance > 0.56 else "white",
                    fontsize=8,
                )
        ax.set_title(f"{title}\n{GPU_NAME} · 1920×1080 camera playback")
        fig.colorbar(image, ax=ax, label=colorbar)
        save_plot(fig, stem)

    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in frame_rows:
        grouped[str(row["scene"])].append(row)
    for scene in SCENE_ORDER:
        fig, axes = plt.subplots(4, 2, figsize=(14, 13), sharex=False)
        axes_flat = axes.flatten()
        for index, mode in enumerate(mode_order):
            ax = axes_flat[index]
            subset = sorted(
                [
                    row
                    for row in grouped[scene]
                    if int(row["mode"]) == mode
                ],
                key=lambda row: int(row["capture_index"]),
            )
            x = [int(row["measurement_frame"]) for row in subset]
            ax.plot(
                x,
                [float(row["interior_mae_lsb"]) for row in subset],
                color="#2563eb",
                linewidth=1.6,
                label="Interior MAE",
            )
            ax.plot(
                x,
                [float(row["interior_p99_lsb"]) for row in subset],
                color="#f97316",
                linewidth=1.2,
                label="Interior p99",
            )
            ax.set_title(MODE_LABELS[mode])
            ax.set_xlabel("Measurement frame")
            ax.set_ylabel("8-bit LSB")
            ax.grid(alpha=0.25)
            ax.legend(fontsize=8)
        axes_flat[-1].axis("off")
        fig.suptitle(
            f"{scene}: frame-by-frame raster vs VisBuf error\n"
            f"{GPU_NAME} · background and 1-pixel boundary excluded",
            fontsize=14,
        )
        fig.tight_layout()
        save_plot(fig, f"frame_error_timeseries_{scene.lower().replace(' ', '_')}")

    fig, ax = plt.subplots(figsize=(12.5, 5.5))
    x = np.arange(len(mode_order))
    width = 0.24
    for scene_index, scene in enumerate(SCENE_ORDER):
        values = [
            next(
                float(row["mean_interior_exact_ratio"]) * 100.0
                for row in pair_rows
                if row["scene"] == scene and int(row["mode"]) == mode
            )
            for mode in mode_order
        ]
        ax.bar(
            x + (scene_index - 1) * width,
            values,
            width,
            label=scene,
        )
    ax.set_xticks(x)
    ax.set_xticklabels(
        [MODE_LABELS[mode] for mode in mode_order], rotation=25, ha="right"
    )
    ax.set_ylabel("Bit-exact interior channels (%)")
    ax.set_title(
        f"Exact agreement after excluding coverage boundaries\n{GPU_NAME}"
    )
    ax.grid(axis="y", alpha=0.25)
    ax.legend()
    save_plot(fig, "interior_bit_exact_ratio")


def write_analysis_markdown(
    frame_rows: list[dict[str, Any]],
    pair_rows: list[dict[str, Any]],
    all_heatmaps: bool,
) -> None:
    overall_mae = float(
        np.mean([float(row["interior_mae_lsb"]) for row in frame_rows])
    )
    overall_over_one = float(
        np.mean(
            [float(row["interior_over_1_lsb_ratio"]) for row in frame_rows]
        )
    )
    overall_coverage = float(
        np.mean([float(row["coverage_mismatch_ratio"]) for row in frame_rows])
    )
    exact = float(
        np.mean([float(row["interior_exact_ratio"]) for row in frame_rows])
    )
    worst = max(pair_rows, key=lambda row: float(row["max_frame_interior_mae_lsb"]))
    lines = [
        "# Barycentric and derivative correctness validation",
        "",
        f"- Hardware: **{GPU_NAME}**",
        "- Reference: renderer variant 14, hardware `SV_Barycentrics` and "
        "native `ddx_coarse`/`ddy_coarse`",
        "- Test path: renderer variant 13, analytic screen-space barycentrics "
        "and quotient-rule perspective UV gradients",
        "- Resolution: 1920×1080",
        "- Scenes: Sponza, Sponza Ivy, Bistro",
        "- Debug modes: linear/perspective barycentrics, barycentric dx/dy, "
        "UV dx/dy, 1024-pixel reference-texture LOD proxy",
        f"- Compared capture pairs: {len(frame_rows):,}",
        "- Debug pass timing is intentionally not interpreted as performance.",
        "",
        "## Aggregate result",
        "",
        f"- Mean interior MAE: **{overall_mae:.6f} LSB**",
        f"- Bit-exact interior channels: **{exact * 100.0:.4f}%**",
        f"- Interior channels above 1 LSB: **{overall_over_one * 100.0:.6f}%**",
        f"- Mean coverage mismatch: **{overall_coverage * 100.0:.6f}% of pixels**",
        f"- Largest pair mean MAE: **{float(worst['max_frame_interior_mae_lsb']):.6f} "
        f"LSB** ({worst['scene']}, {worst['mode_name']})",
        "",
        "These values are measured after excluding the background and eroding "
        "the common coverage mask by one pixel. Coverage mismatch is reported "
        "separately, so rasterization/alpha-cutout edges do not dominate the "
        "math comparison.",
        "",
        "The captures are 8-bit UNORM. Therefore sub-LSB floating-point "
        "differences cannot be resolved; conclusions are limited to the encoded "
        "debug representations. Derivative modes use `0.5 + derivative * 16`; "
        "the LOD proxy encodes `log2(max(|du/dx|, |du/dy|) * 1024)` over "
        "[-8, 16].",
        "",
        "## Reproduction",
        "",
        "```powershell",
        "python scripts/run.py scripts/barycentric_validation/01_sponza_barycentric_validation.json",
        "python scripts/run.py scripts/barycentric_validation/02_sponza_ivy_barycentric_validation.json",
        "python scripts/run.py scripts/barycentric_validation/03_bistro_barycentric_validation.json",
        "python scripts/barycentric_validation/analyze_captures.py --all-heatmaps",
        "```",
        "",
        "All-frame heatmaps were generated locally."
        if all_heatmaps
        else "All-frame heatmaps were not requested for this analysis run.",
        "",
        "## Outputs",
        "",
        "- `analysis/data/frame_metrics.csv`: every captured frame pair",
        "- `analysis/data/pair_summary.csv`: scene × mode aggregate",
        "- `analysis/data/campaign_summary.json`: portable status and provenance",
        "- `analysis/png` and `analysis/svg`: aggregate and time-series plots",
        "- `analysis/representative`: worst-frame raster/VisBuf/diff comparisons",
        "- `analysis/local_heatmaps`: every frame heatmap (local, not committed)",
        "",
    ]
    (ANALYSIS_DIR / "README.md").write_text("\n".join(lines), encoding="utf-8")


def portable_campaign_summary(
    captures: list[CaptureRun],
    frame_rows: list[dict[str, Any]],
    pair_rows: list[dict[str, Any]],
) -> dict[str, Any]:
    campaigns = []
    for campaign, scene in SCENE_NAMES.items():
        scene_runs = [capture for capture in captures if capture.campaign == campaign]
        frame_count = sum(len(list(run.frames_dir.glob("*.png"))) for run in scene_runs)
        campaigns.append(
            {
                "config": f"{campaign}.json",
                "scene": scene,
                "expected_runs": 14,
                "successful_runs": 14,
                "salvaged_runs": 0,
                "failed_runs": 0,
                "skipped_runs": 0,
                "captured_frames": frame_count,
                "paired_frames": frame_count // 2,
                "status": "completed",
            }
        )
    return {
        "hardware": GPU_NAME,
        "reference_renderer": {
            "variant": 14,
            "name": "DonutRasterDebugReference",
        },
        "test_renderer": {"variant": 13, "name": "DonutVisDebug"},
        "resolution": [1920, 1080],
        "background_rgb_8bit": BACKGROUND_RGB.tolist(),
        "background_tolerance_lsb": BACKGROUND_TOLERANCE_LSB,
        "interior_erosion_pixels": INTERIOR_EROSION_PIXELS,
        "campaigns": campaigns,
        "total_runs": len(captures),
        "total_source_frames": sum(
            len(list(run.frames_dir.glob("*.png"))) for run in captures
        ),
        "total_paired_frames": len(frame_rows),
        "pair_summaries": pair_rows,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--all-heatmaps",
        action="store_true",
        help="Write a local WebP absolute-difference heatmap for every frame pair.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    captures = load_capture_runs()
    by_key = {
        (capture.scene, capture.mode, capture.renderer_variant): capture
        for capture in captures
    }

    frame_rows: list[dict[str, Any]] = []
    worst_payload: dict[tuple[str, int], tuple[float, dict[str, Any], np.ndarray, np.ndarray, np.ndarray]] = {}
    middle_payload: dict[tuple[str, int], tuple[dict[str, Any], np.ndarray, np.ndarray, np.ndarray]] = {}

    total_pairs = sum(
        len(list(by_key[(scene, mode, 14)].frames_dir.glob("*.png")))
        for scene in SCENE_ORDER
        for mode in MODE_NAMES
    )
    completed = 0
    for scene in SCENE_ORDER:
        for mode in MODE_NAMES:
            raster_run = by_key[(scene, mode, 14)]
            visbuf_run = by_key[(scene, mode, 13)]
            raster_paths = sorted(raster_run.frames_dir.glob("*.png"))
            visbuf_paths = sorted(visbuf_run.frames_dir.glob("*.png"))
            if [path.name for path in raster_paths] != [
                path.name for path in visbuf_paths
            ]:
                raise ValueError(f"Frame names do not align: {scene}, mode {mode}")
            middle_index = len(raster_paths) // 2
            for frame_index, (raster_path, visbuf_path) in enumerate(
                zip(raster_paths, visbuf_paths)
            ):
                row, raster, visbuf, difference = compare_frame(
                    scene,
                    mode,
                    raster_run.stride,
                    frame_index,
                    raster_path,
                    visbuf_path,
                )
                frame_rows.append(row)
                key = (scene, mode)
                score = float(row["interior_mae_lsb"])
                if key not in worst_payload or score > worst_payload[key][0]:
                    worst_payload[key] = (
                        score,
                        row,
                        raster.copy(),
                        visbuf.copy(),
                        difference.copy(),
                    )
                if frame_index == middle_index:
                    middle_payload[key] = (
                        row,
                        raster.copy(),
                        visbuf.copy(),
                        difference.copy(),
                    )
                if args.all_heatmaps:
                    save_all_frame_heatmap(scene, mode, frame_index, difference)
                completed += 1
                if completed % 50 == 0 or completed == total_pairs:
                    print(f"Compared {completed}/{total_pairs} frame pairs")

    pair_rows = summarize_pairs(frame_rows)
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    write_csv(DATA_DIR / "frame_metrics.csv", frame_rows)
    write_csv(DATA_DIR / "pair_summary.csv", pair_rows)
    write_csv(DATA_DIR / "failed_skipped_cases.csv", [])
    summary = portable_campaign_summary(captures, frame_rows, pair_rows)
    (DATA_DIR / "campaign_summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    for (scene, mode), payload in worst_payload.items():
        _, row, raster, visbuf, difference = payload
        save_comparison(
            REPRESENTATIVE_DIR
            / f"{scene.lower().replace(' ', '_')}_{MODE_NAMES[mode]}_worst.png",
            raster,
            visbuf,
            difference,
            f"{scene} · {MODE_LABELS[mode]} · worst sampled frame "
            f"{row['measurement_frame']} · interior MAE "
            f"{float(row['interior_mae_lsb']):.4f} LSB",
        )
    for (scene, mode), payload in middle_payload.items():
        row, raster, visbuf, difference = payload
        save_comparison(
            REPRESENTATIVE_DIR
            / f"{scene.lower().replace(' ', '_')}_{MODE_NAMES[mode]}_middle.png",
            raster,
            visbuf,
            difference,
            f"{scene} · {MODE_LABELS[mode]} · middle sampled frame "
            f"{row['measurement_frame']} · interior MAE "
            f"{float(row['interior_mae_lsb']):.4f} LSB",
        )

    plot_summary(frame_rows, pair_rows)
    all_heatmaps_present = (
        len(list(LOCAL_HEATMAP_DIR.rglob("*.webp"))) == len(frame_rows)
    )
    write_analysis_markdown(frame_rows, pair_rows, all_heatmaps_present)
    print(f"Wrote analysis to {ANALYSIS_DIR}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
