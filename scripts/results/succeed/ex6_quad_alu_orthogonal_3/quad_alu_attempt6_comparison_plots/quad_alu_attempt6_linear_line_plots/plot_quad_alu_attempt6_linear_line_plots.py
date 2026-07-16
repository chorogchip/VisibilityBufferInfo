#!/usr/bin/env python3
"""Generate linear-x / linear-y versions of every line chart used in attempt 6.

Outputs:
1. Total-only ALU-axis slices
2. Total-only waste-quad-axis slices
3. Total + per-pass ALU-axis slices
4. Total + per-pass waste-quad-axis slices
5. Pairwise crossover-threshold line chart

The data loading, renderer metadata, and crossover estimation are reused from
plot_quad_alu_attempt6_comparison.py so the legend and renderer styling remain
consistent with the full comparison bundle.
"""
from __future__ import annotations

import argparse
import math
import shutil
import zipfile
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter, MaxNLocator
import numpy as np

import plot_quad_alu_attempt6_comparison as base


def save_figure(fig, output_dir: Path, stem: str) -> list[Path]:
    png = output_dir / f"{stem}.png"
    svg = output_dir / f"{stem}.svg"
    fig.savefig(png, dpi=190, bbox_inches="tight")
    fig.savefig(svg, bbox_inches="tight")
    plt.close(fig)
    return [png, svg]


def apply_linear_axes(ax, x_values, xlabel: str, ylabel: str) -> None:
    """Use true linear coordinate spacing on both axes."""
    ax.set_xscale("linear")
    ax.set_yscale("linear")
    ax.set_xlim(float(np.min(x_values)), float(np.max(x_values)))
    ax.set_ylim(bottom=0.0)
    ax.xaxis.set_major_locator(MaxNLocator(nbins=8, integer=True))
    ax.yaxis.set_major_locator(MaxNLocator(nbins=8))
    ax.xaxis.set_major_formatter(FuncFormatter(lambda value, _: base.safe_number(value)))
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.grid(True, which="major", linestyle="--", alpha=0.36)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)


def plot_total_only_slices(quad_values, alu_values, grids, output_dir: Path) -> list[Path]:
    files: list[Path] = []

    quad_indices = base.nearest_indices(quad_values, [8, 32, 96, 384])
    fig, axes = plt.subplots(2, 2, figsize=(18.5, 11.2), constrained_layout=True, sharex=True)
    for ax, oi in zip(axes.flat, quad_indices):
        for variant in base.VARIANT_ORDER:
            rule = base.VARIANT_RULES[variant]
            ax.plot(
                alu_values,
                grids[variant][oi, :],
                linestyle="-",
                linewidth=2.5,
                marker=rule["marker"],
                markersize=5.3,
                color=rule["color"],
                label=f"Variant {variant} · {rule['renderer']} · total",
            )
        apply_linear_axes(ax, alu_values, "ALU calculation count", "Average total GPU time (ms)")
        ax.set_title(f"Waste quad count = {base.safe_number(quad_values[oi])}")
    handles, labels = axes.flat[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="center left", bbox_to_anchor=(1.003, 0.5), frameon=True)
    fig.suptitle("Quad Waste × ALU Orthogonal — ALU-axis Total-Time Slices (Linear X/Y)", fontsize=15)
    files.extend(save_figure(fig, output_dir, "01_total_only_alu_axis_slices_linear_xy"))

    alu_indices = base.nearest_indices(alu_values, [0, 2, 16, 64])
    fig, axes = plt.subplots(2, 2, figsize=(18.5, 11.2), constrained_layout=True, sharex=True)
    for ax, ai in zip(axes.flat, alu_indices):
        for variant in base.VARIANT_ORDER:
            rule = base.VARIANT_RULES[variant]
            ax.plot(
                quad_values,
                grids[variant][:, ai],
                linestyle="-",
                linewidth=2.5,
                marker=rule["marker"],
                markersize=5.3,
                color=rule["color"],
                label=f"Variant {variant} · {rule['renderer']} · total",
            )
        apply_linear_axes(ax, quad_values, "Waste quad count", "Average total GPU time (ms)")
        ax.set_title(f"ALU calculation count = {base.safe_number(alu_values[ai])}")
    handles, labels = axes.flat[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="center left", bbox_to_anchor=(1.003, 0.5), frameon=True)
    fig.suptitle("Quad Waste × ALU Orthogonal — Waste-quad-axis Total-Time Slices (Linear X/Y)", fontsize=15)
    files.extend(save_figure(fig, output_dir, "02_total_only_quad_axis_slices_linear_xy"))
    return files


def plot_pass_slices(quad_values, alu_values, grids, pass_grids, output_dir: Path) -> list[Path]:
    files: list[Path] = []

    def plot_variant_and_passes(ax, x_values, variant, total_values, pass_value_getter):
        rule = base.VARIANT_RULES[variant]
        renderer = rule["renderer"]
        ax.plot(
            x_values,
            total_values,
            linestyle="-",
            linewidth=2.8,
            marker=rule["marker"],
            markersize=5.4,
            color=rule["color"],
            label=f"V{variant} · {renderer} · total",
            zorder=3,
        )
        for pass_name, column, linestyle in rule["passes"]:
            ax.plot(
                x_values,
                pass_value_getter(column),
                linestyle=linestyle,
                linewidth=1.65,
                color=rule["color"],
                alpha=0.9,
                label=f"V{variant} · {renderer} · {pass_name}",
                zorder=4,
            )

    quad_indices = base.nearest_indices(quad_values, [8, 32, 96, 384])
    fig, axes = plt.subplots(2, 2, figsize=(18.5, 11.2), constrained_layout=True, sharex=True)
    for ax, oi in zip(axes.flat, quad_indices):
        for variant in base.VARIANT_ORDER:
            plot_variant_and_passes(
                ax,
                alu_values,
                variant,
                grids[variant][oi, :],
                lambda column, variant=variant, oi=oi: pass_grids[variant][column][oi, :],
            )
        apply_linear_axes(ax, alu_values, "ALU calculation count", "Average GPU time (ms)")
        ax.set_title(f"Waste quad count = {base.safe_number(quad_values[oi])}")
    handles, labels = axes.flat[0].get_legend_handles_labels()
    fig.legend(
        handles,
        labels,
        loc="center left",
        bbox_to_anchor=(1.003, 0.5),
        frameon=True,
        fontsize=9,
    )
    fig.suptitle("Quad Waste × ALU Orthogonal — ALU-axis Slices with Pass Times (Linear X/Y)", fontsize=15)
    fig.text(
        0.5,
        0.006,
        "Thick solid + marker: total · Thin dashed/dotted: individual pass · Forward pass coincides with Forward total",
        ha="center",
        fontsize=9,
    )
    files.extend(save_figure(fig, output_dir, "03_alu_axis_slices_with_pass_times_linear_xy"))

    alu_indices = base.nearest_indices(alu_values, [0, 2, 16, 64])
    fig, axes = plt.subplots(2, 2, figsize=(18.5, 11.2), constrained_layout=True, sharex=True)
    for ax, ai in zip(axes.flat, alu_indices):
        for variant in base.VARIANT_ORDER:
            plot_variant_and_passes(
                ax,
                quad_values,
                variant,
                grids[variant][:, ai],
                lambda column, variant=variant, ai=ai: pass_grids[variant][column][:, ai],
            )
        apply_linear_axes(ax, quad_values, "Waste quad count", "Average GPU time (ms)")
        ax.set_title(f"ALU calculation count = {base.safe_number(alu_values[ai])}")
    handles, labels = axes.flat[0].get_legend_handles_labels()
    fig.legend(
        handles,
        labels,
        loc="center left",
        bbox_to_anchor=(1.003, 0.5),
        frameon=True,
        fontsize=9,
    )
    fig.suptitle("Quad Waste × ALU Orthogonal — Waste-quad-axis Slices with Pass Times (Linear X/Y)", fontsize=15)
    fig.text(
        0.5,
        0.006,
        "Thick solid + marker: total · Thin dashed/dotted: individual pass · Forward pass coincides with Forward total",
        ha="center",
        fontsize=9,
    )
    files.extend(save_figure(fig, output_dir, "04_quad_axis_slices_with_pass_times_linear_xy"))
    return files


def plot_crossover_thresholds_linear(quad_values, alu_values, grids, output_dir: Path) -> list[Path]:
    series = [
        (1, 4, "VisBuf becomes faster than Forward", base.VARIANT_RULES[4]["marker"], base.VARIANT_RULES[4]["color"]),
        (1, 2, "ForwardPrepass becomes faster than Forward", base.VARIANT_RULES[2]["marker"], base.VARIANT_RULES[2]["color"]),
        (2, 4, "VisBuf becomes faster than ForwardPrepass", "D", "C3"),
    ]
    rows = []
    fig, ax = plt.subplots(figsize=(14.5, 8.3), constrained_layout=True)
    plotted_values = []
    for left, right, label, marker, color in series:
        thresholds = []
        for oi, quad in enumerate(quad_values):
            threshold = base.estimate_first_crossover(
                alu_values,
                grids[left][oi, :],
                grids[right][oi, :],
            )
            thresholds.append(threshold)
            rows.append(
                {
                    "waste_quad_control": quad,
                    "comparison": label,
                    "estimated_crossover_alu": threshold,
                }
            )
        values = np.asarray(thresholds, dtype=float)
        mask = np.isfinite(values)
        plotted_values.extend(values[mask].tolist())
        ax.plot(
            quad_values[mask],
            values[mask],
            marker=marker,
            markersize=6.0,
            linewidth=2.3,
            color=color,
            label=label,
        )
    ax.set_xscale("linear")
    ax.set_yscale("linear")
    ax.set_xlim(float(quad_values.min()), float(quad_values.max()))
    ax.set_ylim(0.0, max(plotted_values) * 1.08 if plotted_values else float(alu_values.max()))
    ax.xaxis.set_major_locator(MaxNLocator(nbins=9, integer=True))
    ax.yaxis.set_major_locator(MaxNLocator(nbins=8))
    ax.xaxis.set_major_formatter(FuncFormatter(lambda value, _: base.safe_number(value)))
    ax.yaxis.set_major_formatter(FuncFormatter(lambda value, _: base.safe_number(value)))
    ax.set_xlabel("Waste-quad control (geometry division)")
    ax.set_ylabel("Estimated crossover ALU calculation count")
    ax.grid(True, which="major", linestyle="--", alpha=0.36)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.set_title(
        "Quad Waste × ALU Orthogonal — Estimated Pairwise Crossover Thresholds (Linear X/Y)\n"
        "Lower threshold means the alternative wins with a cheaper material shader"
    )
    ax.legend(loc="best", frameon=True)
    ax.text(
        0.01,
        0.015,
        "Display axes are linear. Thresholds remain interpolated between adjacent measured ALU samples in log2(ALU+1) space.",
        transform=ax.transAxes,
        fontsize=9,
        bbox={"facecolor": "white", "edgecolor": "none", "alpha": 0.80, "pad": 2.5},
    )
    base.write_csv(output_dir / "estimated_pairwise_crossover_thresholds_linear_xy.csv", rows)
    return save_figure(fig, output_dir, "05_estimated_pairwise_crossover_thresholds_linear_xy")


def make_contact_sheet(png_files: list[Path], output_path: Path) -> None:
    from PIL import Image, ImageDraw

    images = [Image.open(path).convert("RGB") for path in png_files]
    thumb_width = 900
    thumbs = []
    for image in images:
        scale = thumb_width / image.width
        thumbs.append(image.resize((thumb_width, max(1, int(image.height * scale)))))
    margin = 24
    label_height = 42
    width = thumb_width + margin * 2
    height = margin + sum(image.height + label_height + margin for image in thumbs)
    sheet = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(sheet)
    y = margin
    for path, image in zip(png_files, thumbs):
        draw.text((margin, y), path.stem, fill="black")
        y += label_height
        sheet.paste(image, (margin, y))
        y += image.height + margin
    sheet.save(output_path, quality=92)


def create_zip(output_dir: Path, csv_path: Path, scripts: list[Path]) -> Path:
    for source in [csv_path, *scripts]:
        target = output_dir / source.name
        if source.resolve() != target.resolve():
            shutil.copy2(source, target)
    zip_path = output_dir.parent / f"{output_dir.name}_bundle.zip"
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(output_dir.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(output_dir.parent))
    return zip_path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path", type=Path)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("quad_alu_attempt6_linear_line_plots"),
    )
    args = parser.parse_args()

    csv_path = args.csv_path.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    raw_rows, _, _ = base.load_csv(csv_path)
    rows = base.aggregate(raw_rows)
    quad_values, alu_values, grids, pass_grids, _ = base.build_grid(rows)

    image_files: list[Path] = []
    image_files.extend(plot_total_only_slices(quad_values, alu_values, grids, output_dir))
    image_files.extend(plot_pass_slices(quad_values, alu_values, grids, pass_grids, output_dir))
    image_files.extend(plot_crossover_thresholds_linear(quad_values, alu_values, grids, output_dir))

    png_files = [path for path in image_files if path.suffix.lower() == ".png"]
    contact_sheet = output_dir / "00_contact_sheet_linear_line_plots.png"
    make_contact_sheet(png_files, contact_sheet)

    summary = [
        "Experiment Attempt 6 — Linear-axis line plots",
        "===============================================",
        "Both x and y axes use linear scales in every graph in this bundle.",
        "The original logarithmic-axis graphs are preserved in the main comparison bundle.",
        "",
        "Included graphs:",
        "- 01: total-only ALU-axis slices",
        "- 02: total-only waste-quad-axis slices",
        "- 03: ALU-axis slices with total and per-pass times",
        "- 04: waste-quad-axis slices with total and per-pass times",
        "- 05: pairwise crossover threshold curves",
        "",
        "Caution:",
        "- On a linear x-axis, low-valued samples cluster near the origin because the sampled controls grow approximately geometrically.",
        "- Crossover threshold values use the same interpolation method as the original plot; only the displayed axes were changed to linear.",
    ]
    (output_dir / "README_linear_axes.txt").write_text("\n".join(summary), encoding="utf-8")

    zip_path = create_zip(
        output_dir,
        csv_path,
        [Path(__file__).resolve(), Path(base.__file__).resolve()],
    )
    print(f"Logical line graphs generated: {len(png_files)}")
    print(f"PNG/SVG files generated: {len(image_files)}")
    print(f"Output directory: {output_dir}")
    print(f"Bundle ZIP: {zip_path}")


if __name__ == "__main__":
    main()
