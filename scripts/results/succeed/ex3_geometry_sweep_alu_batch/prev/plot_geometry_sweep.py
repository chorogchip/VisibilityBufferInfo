
#!/usr/bin/env python3
"""
Generate geometry sweep plots from a TVBPerf batch CSV.

Outputs:
1) 36 plots: material_count (3) × sort_type (3) × format (4),
   comparing variants 1, 2, and 4 with total and individual pass lines.
2) 12 plots: material_count (3) × format (4),
   comparing all sort types for variant 2 (ForwardPrepass), with
   total, depth_prepass, and forward lines for each sort type.
3) PNG and SVG for every plot.
4) Validation and failed-run reports.
5) A ZIP containing all PNG/SVG files.

Usage:
    python plot_geometry_sweep.py input.csv --output-dir geometry_sweep_plots
"""

from __future__ import annotations

import argparse
import math
import re
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.ticker import FuncFormatter
import numpy as np
import pandas as pd


VARIANT_RULES = {
    1: {
        "renderer": "Forward",
        "passes": [
            ("forward", "pass_0_time_avg_ms", "--"),
        ],
        "total": "pass_3_time_avg_ms",
    },
    2: {
        "renderer": "ForwardPrepass",
        "passes": [
            ("depth_prepass", "pass_0_time_avg_ms", "--"),
            ("forward", "pass_1_time_avg_ms", ":"),
        ],
        "total": "pass_3_time_avg_ms",
    },
    4: {
        "renderer": "VisBuf",
        "passes": [
            ("visibility", "pass_0_time_avg_ms", "--"),
            ("resolve", "pass_1_time_avg_ms", ":"),
        ],
        "total": "pass_3_time_avg_ms",
    },
}

PLOT_FORMATS = [
    {
        "number": "01",
        "stem": "log_x_log_y_pass_time",
        "title": "Geometry Sweep — Pass Time by Variant (Log X / Log Y)",
        "normalized": False,
        "log_x": True,
        "log_y": True,
        "ylabel": "Pass time (ms)",
    },
    {
        "number": "02",
        "stem": "linear_x_linear_y_pass_time",
        "title": "Geometry Sweep — Pass Time by Variant (Linear X / Linear Y)",
        "normalized": False,
        "log_x": False,
        "log_y": False,
        "ylabel": "Pass time (ms)",
    },
    {
        "number": "03",
        "stem": "linear_x_linear_y_time_per_object",
        "title": "Geometry Sweep — Pass Time per Object (Linear X / Linear Y)",
        "normalized": True,
        "log_x": False,
        "log_y": False,
        "ylabel": "Pass time per object (ms/object)",
    },
    {
        "number": "04",
        "stem": "log_x_log_y_time_per_object",
        "title": "Geometry Sweep — Pass Time per Object (Log X / Log Y)",
        "normalized": True,
        "log_x": True,
        "log_y": True,
        "ylabel": "Pass time per object (ms/object)",
    },
]

TOTAL_LINEWIDTH = 2.8
TOTAL_MARKERSIZE = 5.8
PASS_LINEWIDTH = 1.5
PASS_ALPHA = 0.34
VALIDATION_TOLERANCE_MS = 1.1e-5


@dataclass(frozen=True)
class Columns:
    status: str
    variant: str
    renderer_name: str
    object_count: str
    material_count: str
    sort_type: str


def resolve_column(df: pd.DataFrame, candidates: Iterable[str], required: bool = True) -> str | None:
    """Resolve a column using exact and normalized name matching."""
    for candidate in candidates:
        if candidate in df.columns:
            return candidate

    normalized = {
        re.sub(r"[^a-z0-9]+", "", str(col).lower()): col
        for col in df.columns
    }
    for candidate in candidates:
        key = re.sub(r"[^a-z0-9]+", "", candidate.lower())
        if key in normalized:
            return normalized[key]

    if required:
        raise KeyError(f"Required column not found. Tried: {list(candidates)}")
    return None


def resolve_columns(df: pd.DataFrame) -> Columns:
    return Columns(
        status=resolve_column(df, ["runner_status", "status"]),
        variant=resolve_column(df, ["renderer-variant", "renderer_variant", "variant"]),
        renderer_name=resolve_column(df, ["renderer_name", "renderer-name"], required=False) or "",
        object_count=resolve_column(df, ["object-count", "object_count"]),
        material_count=resolve_column(df, ["material-count", "material_count"]),
        sort_type=resolve_column(df, ["sort-type", "sort_type"]),
    )


def load_and_clean(csv_path: Path) -> tuple[pd.DataFrame, pd.DataFrame, Columns]:
    raw = pd.read_csv(csv_path)
    cols = resolve_columns(raw)

    success_mask = raw[cols.status].astype(str).str.lower().eq("success")
    success = raw.loc[success_mask].copy()
    failed = raw.loc[~success_mask].copy()

    numeric_cols = [
        cols.variant,
        cols.object_count,
        cols.material_count,
        cols.sort_type,
        "pass_0_time_avg_ms",
        "pass_1_time_avg_ms",
        "pass_2_time_avg_ms",
        "pass_3_time_avg_ms",
    ]
    for col in numeric_cols:
        if col in success.columns:
            success[col] = pd.to_numeric(success[col], errors="coerce")

    success = success.dropna(
        subset=[cols.variant, cols.object_count, cols.material_count, cols.sort_type]
    )
    success[cols.variant] = success[cols.variant].astype(int)
    success[cols.object_count] = success[cols.object_count].astype(int)
    success[cols.material_count] = success[cols.material_count].astype(int)
    success[cols.sort_type] = success[cols.sort_type].astype(int)

    supported = set(VARIANT_RULES)
    success = success[success[cols.variant].isin(supported)].copy()

    return success, failed, cols


def active_passes_by_variant(df: pd.DataFrame, cols: Columns) -> dict[int, list[tuple[str, str, str]]]:
    result: dict[int, list[tuple[str, str, str]]] = {}
    for variant, rule in VARIANT_RULES.items():
        variant_df = df[df[cols.variant] == variant]
        active = []
        for pass_name, column, linestyle in rule["passes"]:
            if column in variant_df.columns:
                values = pd.to_numeric(variant_df[column], errors="coerce")
                if (values > 0).any():
                    active.append((pass_name, column, linestyle))
        result[variant] = active
    return result


def validate_totals(df: pd.DataFrame, cols: Columns, output_dir: Path) -> pd.DataFrame:
    rows = []
    detailed = []

    for variant, rule in VARIANT_RULES.items():
        variant_df = df[df[cols.variant] == variant].copy()
        pass_columns = [column for _, column, _ in rule["passes"] if column in variant_df.columns]
        individual_sum = variant_df[pass_columns].fillna(0).sum(axis=1)
        errors = (variant_df[rule["total"]] - individual_sum).abs()

        renderer = rule["renderer"]
        max_error = float(errors.max()) if len(errors) else math.nan
        mean_error = float(errors.mean()) if len(errors) else math.nan
        above = int((errors > VALIDATION_TOLERANCE_MS).sum()) if len(errors) else 0

        rows.append(
            {
                "variant": variant,
                "renderer": renderer,
                "successful_rows": len(variant_df),
                "max_total_error_ms": max_error,
                "mean_total_error_ms": mean_error,
                "rows_above_tolerance": above,
                "tolerance_ms": VALIDATION_TOLERANCE_MS,
                "status": "WARNING" if above else "OK",
            }
        )

        detail_cols = [
            cols.material_count,
            cols.sort_type,
            cols.object_count,
        ]
        detail = variant_df[detail_cols].copy()
        detail.insert(0, "variant", variant)
        detail.insert(1, "renderer", renderer)
        detail["total_ms"] = variant_df[rule["total"]].to_numpy()
        detail["individual_pass_sum_ms"] = individual_sum.to_numpy()
        detail["total_error_ms"] = errors.to_numpy()
        detailed.append(detail)

    summary = pd.DataFrame(rows)
    summary.to_csv(output_dir / "validation_summary.csv", index=False)
    pd.concat(detailed, ignore_index=True).to_csv(
        output_dir / "validation_details.csv", index=False
    )

    print("Total validation by variant:")
    for row in rows:
        print(
            f"  Variant {row['variant']} · {row['renderer']}: "
            f"max error = {row['max_total_error_ms']:.12g} ms, "
            f"rows above {VALIDATION_TOLERANCE_MS:.2g} ms = "
            f"{row['rows_above_tolerance']} [{row['status']}]"
        )
    return summary


def export_failed_runs(failed: pd.DataFrame, output_dir: Path) -> pd.DataFrame:
    available = []
    desired = [
        "runner_status",
        "runner_error",
        "param_renderer_variant",
        "param_material_count",
        "param_sort_type",
        "param_object_count",
        "renderer-variant",
        "material-count",
        "sort-type",
        "object-count",
    ]
    for col in desired:
        if col in failed.columns:
            available.append(col)

    report = failed[available].copy() if available else failed.copy()
    report.to_csv(output_dir / "failed_runs.csv", index=False)
    return report


def thousands_formatter(value: float, _position: int) -> str:
    if not np.isfinite(value):
        return ""
    if abs(value - round(value)) < 1e-8:
        return f"{int(round(value)):,}"
    return f"{value:,.3g}"


def apply_axis_style(
    ax: plt.Axes,
    *,
    x_values: np.ndarray,
    y_values: list[np.ndarray],
    log_x: bool,
    log_y: bool,
) -> None:
    positive_x = x_values[np.isfinite(x_values) & (x_values > 0)]

    if log_x:
        ax.set_xscale("log", base=2)
        max_x = int(np.nanmax(positive_x))
        exponent_max = int(math.ceil(math.log(max_x, 4))) if max_x > 1 else 0
        ticks = [4**exponent for exponent in range(0, exponent_max + 1)]
        ticks = [tick for tick in ticks if tick <= max_x]
        if 1 not in ticks:
            ticks.insert(0, 1)
        ax.set_xticks(ticks)
        ax.set_xlim(1, max_x * 1.16)
    else:
        ax.set_xlim(left=0)
        ax.set_xticks(sorted(set(int(v) for v in positive_x)))

    ax.xaxis.set_major_formatter(FuncFormatter(thousands_formatter))

    finite_y = np.concatenate(
        [np.asarray(values, dtype=float) for values in y_values if len(values)]
    )
    finite_y = finite_y[np.isfinite(finite_y)]

    if log_y:
        ax.set_yscale("log")
        positive_y = finite_y[finite_y > 0]
        if len(positive_y):
            ax.set_ylim(positive_y.min() * 0.78, positive_y.max() * 1.28)
    else:
        ax.set_ylim(bottom=0)
        if len(finite_y):
            top = finite_y.max()
            ax.set_ylim(0, top * 1.08 if top > 0 else 1)

    ax.grid(True, which="major", linestyle="--", alpha=0.4)
    ax.grid(True, which="minor", linestyle=":", alpha=0.2)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)


def plot_series(
    ax: plt.Axes,
    *,
    x: np.ndarray,
    y: np.ndarray,
    color,
    label: str,
    is_total: bool,
    linestyle: str,
    log_y: bool,
) -> Line2D | None:
    mask = np.isfinite(x) & np.isfinite(y)
    if log_y:
        mask &= y > 0
    mask &= x > 0

    if not mask.any():
        return None

    if is_total:
        (line,) = ax.plot(
            x[mask],
            y[mask],
            color=color,
            linestyle="-",
            linewidth=TOTAL_LINEWIDTH,
            marker="o",
            markersize=TOTAL_MARKERSIZE,
            label=label,
            zorder=3,
        )
    else:
        (line,) = ax.plot(
            x[mask],
            y[mask],
            color=color,
            linestyle=linestyle,
            linewidth=PASS_LINEWIDTH,
            alpha=PASS_ALPHA,
            label=label,
            zorder=2,
        )
    return line


def aggregate_group(group: pd.DataFrame, cols: Columns) -> pd.DataFrame:
    value_cols = [
        column
        for column in [
            "pass_0_time_avg_ms",
            "pass_1_time_avg_ms",
            "pass_2_time_avg_ms",
            "pass_3_time_avg_ms",
        ]
        if column in group.columns
    ]
    return (
        group.groupby(cols.object_count, as_index=False)[value_cols]
        .mean(numeric_only=True)
        .sort_values(cols.object_count)
    )


def save_figure(fig: plt.Figure, base_path: Path) -> list[Path]:
    base_path.parent.mkdir(parents=True, exist_ok=True)
    png = base_path.with_suffix(".png")
    svg = base_path.with_suffix(".svg")
    if png.exists() and svg.exists() and png.stat().st_size > 0 and svg.stat().st_size > 0:
        plt.close(fig)
        return [png, svg]
    fig.savefig(png, dpi=180, bbox_inches="tight")
    fig.savefig(svg, bbox_inches="tight")
    plt.close(fig)
    return [png, svg]


def plot_variant_comparison(
    df: pd.DataFrame,
    cols: Columns,
    active_passes: dict[int, list[tuple[str, str, str]]],
    material_count: int,
    sort_type: int,
    spec: dict,
    output_dir: Path,
) -> list[Path]:
    subset = df[
        (df[cols.material_count] == material_count)
        & (df[cols.sort_type] == sort_type)
    ]

    fig, ax = plt.subplots(figsize=(14.5, 8.4), constrained_layout=True)
    colors = plt.rcParams["axes.prop_cycle"].by_key()["color"]
    handles: list[Line2D] = []
    labels: list[str] = []
    all_y: list[np.ndarray] = []
    all_x: list[np.ndarray] = []

    for color_index, variant in enumerate(VARIANT_RULES):
        rule = VARIANT_RULES[variant]
        group = subset[subset[cols.variant] == variant]
        if group.empty:
            continue

        agg = aggregate_group(group, cols)
        x = agg[cols.object_count].to_numpy(dtype=float)
        color = colors[color_index % len(colors)]
        all_x.append(x)

        series = [("total", rule["total"], "-", True)]
        series.extend(
            (pass_name, column, linestyle, False)
            for pass_name, column, linestyle in active_passes[variant]
        )

        for pass_name, column, linestyle, is_total in series:
            if column not in agg.columns:
                continue
            y = agg[column].to_numpy(dtype=float)
            if spec["normalized"]:
                y = np.divide(
                    y,
                    x,
                    out=np.full_like(y, np.nan, dtype=float),
                    where=x != 0,
                )
            all_y.append(y)
            label = f"Variant {variant} · {rule['renderer']} · {pass_name}"
            line = plot_series(
                ax,
                x=x,
                y=y,
                color=color,
                label=label,
                is_total=is_total,
                linestyle=linestyle,
                log_y=spec["log_y"],
            )
            if line is not None:
                handles.append(line)
                labels.append(label)

    if not all_x:
        plt.close(fig)
        return []

    apply_axis_style(
        ax,
        x_values=np.unique(np.concatenate(all_x)),
        y_values=all_y,
        log_x=spec["log_x"],
        log_y=spec["log_y"],
    )
    ax.set_xlabel("Object count")
    ax.set_ylabel(spec["ylabel"])
    ax.set_title(
        f"{spec['title']}\n"
        f"Material Count: {material_count:,} · Sort Type: {sort_type}"
    )
    ax.legend(
        handles,
        labels,
        loc="upper left",
        ncol=2 if len(labels) > 6 else 1,
        frameon=True,
    )
    ax.text(
        0.01,
        0.015,
        "Bold solid lines: total · Faint dashed lines: non-zero passes",
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=9,
        bbox={"facecolor": "white", "edgecolor": "none", "alpha": 0.72, "pad": 2.5},
        zorder=10,
    )

    base = (
        output_dir
        / "by_material_and_sort"
        / f"material_{material_count}"
        / f"sort_{sort_type}"
        / f"{spec['number']}_{spec['stem']}"
    )
    return save_figure(fig, base)


def plot_forward_prepass_sort_comparison(
    df: pd.DataFrame,
    cols: Columns,
    active_passes: dict[int, list[tuple[str, str, str]]],
    material_count: int,
    spec: dict,
    output_dir: Path,
) -> list[Path]:
    variant = 2
    rule = VARIANT_RULES[variant]
    subset = df[
        (df[cols.material_count] == material_count)
        & (df[cols.variant] == variant)
    ]

    fig, ax = plt.subplots(figsize=(14.5, 8.4), constrained_layout=True)
    colors = plt.rcParams["axes.prop_cycle"].by_key()["color"]
    handles: list[Line2D] = []
    labels: list[str] = []
    all_y: list[np.ndarray] = []
    all_x: list[np.ndarray] = []

    sort_types = sorted(subset[cols.sort_type].dropna().astype(int).unique())
    for color_index, sort_type in enumerate(sort_types):
        group = subset[subset[cols.sort_type] == sort_type]
        agg = aggregate_group(group, cols)
        x = agg[cols.object_count].to_numpy(dtype=float)
        color = colors[color_index % len(colors)]
        all_x.append(x)

        series = [("total", rule["total"], "-", True)]
        series.extend(
            (pass_name, column, linestyle, False)
            for pass_name, column, linestyle in active_passes[variant]
        )

        for pass_name, column, linestyle, is_total in series:
            if column not in agg.columns:
                continue
            y = agg[column].to_numpy(dtype=float)
            if spec["normalized"]:
                y = np.divide(
                    y,
                    x,
                    out=np.full_like(y, np.nan, dtype=float),
                    where=x != 0,
                )
            all_y.append(y)
            label = f"Sort {sort_type} · {rule['renderer']} · {pass_name}"
            line = plot_series(
                ax,
                x=x,
                y=y,
                color=color,
                label=label,
                is_total=is_total,
                linestyle=linestyle,
                log_y=spec["log_y"],
            )
            if line is not None:
                handles.append(line)
                labels.append(label)

    if not all_x:
        plt.close(fig)
        return []

    apply_axis_style(
        ax,
        x_values=np.unique(np.concatenate(all_x)),
        y_values=all_y,
        log_x=spec["log_x"],
        log_y=spec["log_y"],
    )
    ax.set_xlabel("Object count")
    ax.set_ylabel(spec["ylabel"])

    title = spec["title"].replace("by Variant", "for ForwardPrepass by Sort Type")
    ax.set_title(f"{title}\nMaterial Count: {material_count:,}")
    ax.legend(
        handles,
        labels,
        loc="upper left",
        ncol=2 if len(labels) > 6 else 1,
        frameon=True,
    )
    ax.text(
        0.01,
        0.015,
        "Bold solid lines: total · Faint dashed lines: non-zero passes",
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=9,
        bbox={"facecolor": "white", "edgecolor": "none", "alpha": 0.72, "pad": 2.5},
        zorder=10,
    )

    base = (
        output_dir
        / "forward_prepass_by_sort"
        / f"material_{material_count}"
        / f"{spec['number']}_{spec['stem']}"
    )
    return save_figure(fig, base)


def write_run_summary(
    output_dir: Path,
    df: pd.DataFrame,
    failed: pd.DataFrame,
    cols: Columns,
    plot_files: list[Path],
    validation: pd.DataFrame,
) -> None:
    materials = [int(v) for v in sorted(df[cols.material_count].unique())]
    sorts = [int(v) for v in sorted(df[cols.sort_type].unique())]
    objects = [int(v) for v in sorted(df[cols.object_count].unique())]
    variants = [int(v) for v in sorted(df[cols.variant].unique())]

    lines = [
        "Geometry Sweep Plot Generation Summary",
        "======================================",
        f"Successful rows used: {len(df)}",
        f"Non-success rows: {len(failed)}",
        f"Materials: {materials}",
        f"Sort types: {sorts}",
        f"Object counts: {objects}",
        f"Variants: {variants}",
        f"Logical graphs generated: {len(plot_files) // 2}",
        f"Image/vector files generated: {len(plot_files)}",
        "",
        "Validation:",
    ]
    for row in validation.to_dict("records"):
        lines.append(
            f"- Variant {row['variant']} · {row['renderer']}: "
            f"max error {row['max_total_error_ms']:.12g} ms; "
            f"status {row['status']}"
        )

    lines.extend(
        [
            "",
            "Known missing measurements:",
            "- Inspect failed_runs.csv. The supplied dataset contains six failed runs",
            "  for Variant 1, sort_type 2, object_count 4,096 and 16,384",
            "  across material counts 10, 20, and 30.",
        ]
    )
    (output_dir / "run_summary.txt").write_text("\n".join(lines), encoding="utf-8")


def create_images_zip(output_dir: Path, files: list[Path]) -> Path:
    zip_path = output_dir.parent / f"{output_dir.name}_png_svg.zip"
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            archive.write(path, path.relative_to(output_dir))
    return zip_path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path", type=Path)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("geometry_sweep_plots"),
    )
    args = parser.parse_args()

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    df, failed, cols = load_and_clean(args.csv_path)
    active_passes = active_passes_by_variant(df, cols)
    validation = validate_totals(df, cols, output_dir)
    export_failed_runs(failed, output_dir)

    materials = sorted(df[cols.material_count].unique())
    sort_types = sorted(df[cols.sort_type].unique())
    plot_files: list[Path] = []

    for material_count in materials:
        for sort_type in sort_types:
            for spec in PLOT_FORMATS:
                plot_files.extend(
                    plot_variant_comparison(
                        df,
                        cols,
                        active_passes,
                        material_count,
                        sort_type,
                        spec,
                        output_dir,
                    )
                )

    for material_count in materials:
        for spec in PLOT_FORMATS:
            plot_files.extend(
                plot_forward_prepass_sort_comparison(
                    df,
                    cols,
                    active_passes,
                    material_count,
                    spec,
                    output_dir,
                )
            )

    write_run_summary(
        output_dir,
        df,
        failed,
        cols,
        plot_files,
        validation,
    )
    zip_path = create_images_zip(output_dir, plot_files)

    print()
    print(f"Logical graphs generated: {len(plot_files) // 2}")
    print(f"PNG/SVG files generated: {len(plot_files)}")
    print(f"Output directory: {output_dir}")
    print(f"Images ZIP: {zip_path}")


if __name__ == "__main__":
    main()
