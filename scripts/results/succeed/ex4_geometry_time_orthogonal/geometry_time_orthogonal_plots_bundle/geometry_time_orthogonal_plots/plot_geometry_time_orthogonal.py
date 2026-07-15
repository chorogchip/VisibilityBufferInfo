#!/usr/bin/env python3
"""
Plot a geometry_time_orthogonal TVBPerf CSV.

The experiment has one swept numeric dimension (object_count) and three
renderer variants, so this script creates four all-in-one graphs:

1. Average pass time, log X / log Y
2. Average pass time, linear X / linear Y
3. Average pass time per object, log X / log Y
4. Total time relative to Forward, log X / linear Y

Every graph contains all variants, their total time, and every active
individual pass. PNG and SVG versions, validation reports, and a ZIP bundle
are generated.

Usage:
    python plot_geometry_time_orthogonal.py geometry_time_orthogonal.csv \
        --output-dir geometry_time_orthogonal_plots
"""

from __future__ import annotations

import argparse
import csv
import math
import zipfile
from pathlib import Path
from typing import Iterable

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
import numpy as np


VARIANT_RULES = {
    1: {
        "renderer": "Forward",
        "marker": "o",
        "passes": [
            ("forward", "pass_0_time_avg_ms", "--"),
        ],
        "total": "pass_3_time_avg_ms",
    },
    2: {
        "renderer": "ForwardPrepass",
        "marker": "s",
        "passes": [
            ("depth_prepass", "pass_0_time_avg_ms", "--"),
            ("forward", "pass_1_time_avg_ms", ":"),
        ],
        "total": "pass_3_time_avg_ms",
    },
    4: {
        "renderer": "VisBuf",
        "marker": "^",
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
        "title": "Geometry Time Orthogonal — Pass Time by Variant (Log X / Log Y)",
        "normalized": False,
        "log_x": True,
        "log_y": True,
        "ylabel": "Average pass time (ms)",
    },
    {
        "number": "02",
        "stem": "linear_x_linear_y_pass_time",
        "title": "Geometry Time Orthogonal — Pass Time by Variant (Linear X / Linear Y)",
        "normalized": False,
        "log_x": False,
        "log_y": False,
        "ylabel": "Average pass time (ms)",
    },
    {
        "number": "03",
        "stem": "log_x_log_y_time_per_object",
        "title": "Geometry Time Orthogonal — Pass Time per Object (Log X / Log Y)",
        "normalized": True,
        "log_x": True,
        "log_y": True,
        "ylabel": "Average pass time per object (ms/object)",
    },
]

VALIDATION_TOLERANCE_MS = 1.1e-5


def normalize_name(name: str) -> str:
    return "".join(ch for ch in name.lower() if ch.isalnum())


def resolve_column(fieldnames: Iterable[str], candidates: Iterable[str]) -> str:
    exact = set(fieldnames)
    for candidate in candidates:
        if candidate in exact:
            return candidate

    normalized = {normalize_name(name): name for name in fieldnames}
    for candidate in candidates:
        key = normalize_name(candidate)
        if key in normalized:
            return normalized[key]

    raise KeyError(f"Required column not found. Tried: {list(candidates)}")


def as_float(value: str | None) -> float:
    try:
        return float(value) if value not in (None, "") else math.nan
    except (TypeError, ValueError):
        return math.nan


def load_csv(csv_path: Path):
    with csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        rows = list(reader)
        fieldnames = reader.fieldnames or []

    status_col = resolve_column(fieldnames, ["runner_status", "status"])
    variant_col = resolve_column(fieldnames, ["renderer-variant", "renderer_variant", "variant"])
    object_col = resolve_column(fieldnames, ["object-count", "object_count"])
    renderer_col = resolve_column(fieldnames, ["renderer_name", "renderer-name"])

    success = [
        row for row in rows
        if str(row.get(status_col, "")).strip().lower() == "success"
    ]
    failed = [
        row for row in rows
        if str(row.get(status_col, "")).strip().lower() != "success"
    ]

    cleaned = []
    for row in success:
        variant = int(as_float(row.get(variant_col)))
        object_count = int(as_float(row.get(object_col)))
        if variant not in VARIANT_RULES or object_count <= 0:
            continue
        cleaned.append(
            {
                "variant": variant,
                "renderer": row.get(renderer_col) or VARIANT_RULES[variant]["renderer"],
                "object_count": object_count,
                **{
                    name: as_float(row.get(name))
                    for name in [
                        "pass_0_time_avg_ms",
                        "pass_1_time_avg_ms",
                        "pass_2_time_avg_ms",
                        "pass_3_time_avg_ms",
                    ]
                },
            }
        )

    return cleaned, failed, fieldnames


def aggregate(rows):
    buckets: dict[tuple[int, int], list[dict]] = {}
    for row in rows:
        buckets.setdefault((row["variant"], row["object_count"]), []).append(row)

    result: dict[int, list[dict]] = {}
    for (variant, object_count), group in buckets.items():
        agg = {"variant": variant, "object_count": object_count}
        for column in [
            "pass_0_time_avg_ms",
            "pass_1_time_avg_ms",
            "pass_2_time_avg_ms",
            "pass_3_time_avg_ms",
        ]:
            values = np.array([item[column] for item in group], dtype=float)
            finite = values[np.isfinite(values)]
            agg[column] = float(finite.mean()) if finite.size else math.nan
        result.setdefault(variant, []).append(agg)

    for variant in result:
        result[variant].sort(key=lambda row: row["object_count"])
    return result


def active_passes(rows_by_variant):
    result = {}
    for variant, rule in VARIANT_RULES.items():
        active = []
        data = rows_by_variant.get(variant, [])
        for pass_name, column, linestyle in rule["passes"]:
            if any(
                math.isfinite(row.get(column, math.nan)) and row[column] > 0
                for row in data
            ):
                active.append((pass_name, column, linestyle))
        result[variant] = active
    return result


def thousands_formatter(value: float, _position: int) -> str:
    if not math.isfinite(value):
        return ""
    if abs(value - round(value)) < 1e-8:
        return f"{int(round(value)):,}"
    return f"{value:,.3g}"


def apply_axis_style(ax, x_values, y_values, log_x, log_y):
    positive_x = np.array([x for x in x_values if math.isfinite(x) and x > 0], dtype=float)

    if log_x:
        ax.set_xscale("log", base=2)
        max_x = int(positive_x.max())
        ticks = []
        value = 1
        while value <= max_x:
            ticks.append(value)
            value *= 4
        if max_x not in ticks:
            ticks.append(max_x)
        ax.set_xticks(sorted(set(ticks)))
        ax.set_xlim(1, max_x * 1.16)
    else:
        max_x = int(positive_x.max())
        ax.set_xlim(0, max_x * 1.03)
        preferred = [0, 1024, 2048, 4096, 8192, 16384]
        ticks = [tick for tick in preferred if tick <= max_x]
        if max_x not in ticks:
            ticks.append(max_x)
        ax.set_xticks(sorted(set(ticks)))

    ax.xaxis.set_major_formatter(FuncFormatter(thousands_formatter))

    finite_y = np.array(
        [value for series in y_values for value in series if math.isfinite(value)],
        dtype=float,
    )

    if log_y:
        ax.set_yscale("log")
        positive_y = finite_y[finite_y > 0]
        if positive_y.size:
            ax.set_ylim(positive_y.min() * 0.78, positive_y.max() * 1.28)
    else:
        top = float(finite_y.max()) if finite_y.size else 1.0
        ax.set_ylim(0, top * 1.08 if top > 0 else 1.0)

    ax.grid(True, which="major", linestyle="--", alpha=0.4)
    ax.grid(True, which="minor", linestyle=":", alpha=0.2)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)


def plot_all_variants(rows_by_variant, pass_map, spec, output_dir):
    fig, ax = plt.subplots(figsize=(14.5, 8.4), constrained_layout=True)
    all_x = []
    all_y = []

    # Individual passes first, then totals so total curves remain visually dominant.
    for variant, rule in VARIANT_RULES.items():
        data = rows_by_variant.get(variant, [])
        if not data:
            continue

        x = np.array([row["object_count"] for row in data], dtype=float)
        all_x.extend(x.tolist())

        for pass_name, column, linestyle in pass_map[variant]:
            y = np.array([row[column] for row in data], dtype=float)
            if spec["normalized"]:
                y = np.divide(
                    y,
                    x,
                    out=np.full_like(y, np.nan),
                    where=x != 0,
                )
            mask = np.isfinite(x) & np.isfinite(y) & (x > 0)
            if spec["log_y"]:
                mask &= y > 0
            if not mask.any():
                continue
            all_y.append(y[mask].tolist())
            ax.plot(
                x[mask],
                y[mask],
                linestyle=linestyle,
                linewidth=1.6,
                alpha=0.58,
                label=f"Variant {variant} · {rule['renderer']} · {pass_name}",
                zorder=2,
            )

    for variant, rule in VARIANT_RULES.items():
        data = rows_by_variant.get(variant, [])
        if not data:
            continue

        x = np.array([row["object_count"] for row in data], dtype=float)
        y = np.array([row[rule["total"]] for row in data], dtype=float)
        if spec["normalized"]:
            y = np.divide(
                y,
                x,
                out=np.full_like(y, np.nan),
                where=x != 0,
            )
        mask = np.isfinite(x) & np.isfinite(y) & (x > 0)
        if spec["log_y"]:
            mask &= y > 0
        if not mask.any():
            continue
        all_y.append(y[mask].tolist())
        ax.plot(
            x[mask],
            y[mask],
            linestyle="-",
            linewidth=2.9,
            marker=rule["marker"],
            markersize=5.8,
            label=f"Variant {variant} · {rule['renderer']} · total",
            zorder=4,
        )

    apply_axis_style(
        ax,
        x_values=all_x,
        y_values=all_y,
        log_x=spec["log_x"],
        log_y=spec["log_y"],
    )

    ax.set_xlabel("Object count")
    ax.set_ylabel(spec["ylabel"])
    ax.set_title(spec["title"])
    ax.legend(loc="upper left", ncol=2, frameon=True, fontsize=9.2)
    ax.text(
        0.01,
        0.014,
        "Bold solid lines: total · Thin dashed/dotted lines: individual passes · "
        "Variant 1 forward equals and overlaps its total",
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=9,
        bbox={"facecolor": "white", "edgecolor": "none", "alpha": 0.76, "pad": 2.5},
        zorder=10,
    )

    base = output_dir / f"{spec['number']}_{spec['stem']}"
    png = base.with_suffix(".png")
    svg = base.with_suffix(".svg")
    fig.savefig(png, dpi=180, bbox_inches="tight")
    fig.savefig(svg, bbox_inches="tight")
    plt.close(fig)
    return [png, svg]



def plot_total_relative_to_forward(rows_by_variant, output_dir):
    baseline_rows = rows_by_variant.get(1, [])
    baseline = {
        row["object_count"]: row[VARIANT_RULES[1]["total"]]
        for row in baseline_rows
        if math.isfinite(row[VARIANT_RULES[1]["total"]])
        and row[VARIANT_RULES[1]["total"]] > 0
    }

    fig, ax = plt.subplots(figsize=(14.5, 8.4), constrained_layout=True)
    all_x = []
    all_y = []

    for variant, rule in VARIANT_RULES.items():
        data = rows_by_variant.get(variant, [])
        x_values = []
        ratios = []
        for row in data:
            object_count = row["object_count"]
            total = row[rule["total"]]
            forward_total = baseline.get(object_count, math.nan)
            if (
                object_count > 0
                and math.isfinite(total)
                and math.isfinite(forward_total)
                and forward_total > 0
            ):
                x_values.append(float(object_count))
                ratios.append(float(total / forward_total))

        if not x_values:
            continue

        x = np.array(x_values, dtype=float)
        y = np.array(ratios, dtype=float)
        all_x.extend(x.tolist())
        all_y.append(y.tolist())
        ax.plot(
            x,
            y,
            linestyle="-",
            linewidth=2.7,
            marker=rule["marker"],
            markersize=5.8,
            label=f"Variant {variant} · {rule['renderer']} · total / Forward",
            zorder=3,
        )

    apply_axis_style(
        ax,
        x_values=all_x,
        y_values=all_y,
        log_x=True,
        log_y=False,
    )
    ax.set_ylim(bottom=0)
    ax.set_xlabel("Object count")
    ax.set_ylabel("Total time relative to Forward (×)")
    ax.set_title(
        "Geometry Time Orthogonal — Total Time Relative to Forward "
        "(Log X / Linear Y)"
    )
    ax.legend(loc="upper right", frameon=True)
    ax.text(
        0.01,
        0.014,
        "1.0× equals Forward · Values above 1.0× are slower than Forward",
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=9,
        bbox={"facecolor": "white", "edgecolor": "none", "alpha": 0.76, "pad": 2.5},
        zorder=10,
    )

    base = output_dir / "04_log_x_linear_y_total_relative_to_forward"
    png = base.with_suffix(".png")
    svg = base.with_suffix(".svg")
    fig.savefig(png, dpi=180, bbox_inches="tight")
    fig.savefig(svg, bbox_inches="tight")
    plt.close(fig)
    return [png, svg]


def validate_totals(rows_by_variant, output_dir):
    summary_rows = []
    detail_rows = []

    for variant, rule in VARIANT_RULES.items():
        for row in rows_by_variant.get(variant, []):
            pass_columns = [column for _, column, _ in rule["passes"]]
            pass_sum = sum(
                row[column]
                for column in pass_columns
                if math.isfinite(row.get(column, math.nan))
            )
            total = row[rule["total"]]
            error = abs(total - pass_sum)
            detail_rows.append(
                {
                    "variant": variant,
                    "renderer": rule["renderer"],
                    "object_count": row["object_count"],
                    "total_ms": total,
                    "individual_pass_sum_ms": pass_sum,
                    "total_error_ms": error,
                }
            )

        errors = [
            detail["total_error_ms"]
            for detail in detail_rows
            if detail["variant"] == variant
        ]
        max_error = max(errors) if errors else math.nan
        mean_error = sum(errors) / len(errors) if errors else math.nan
        above = sum(error > VALIDATION_TOLERANCE_MS for error in errors)
        summary_rows.append(
            {
                "variant": variant,
                "renderer": rule["renderer"],
                "successful_rows": len(rows_by_variant.get(variant, [])),
                "max_total_error_ms": max_error,
                "mean_total_error_ms": mean_error,
                "rows_above_tolerance": above,
                "tolerance_ms": VALIDATION_TOLERANCE_MS,
                "status": "WARNING" if above else "OK",
            }
        )

    def write_csv(path, rows):
        if not rows:
            return
        with path.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)

    write_csv(output_dir / "validation_summary.csv", summary_rows)
    write_csv(output_dir / "validation_details.csv", detail_rows)
    return summary_rows


def export_failed_runs(failed, fieldnames, output_dir):
    preferred = [
        "runner_status",
        "runner_return_code",
        "runner_error",
        "param_object_count",
        "param_renderer_variant",
        "object-count",
        "renderer-variant",
    ]
    columns = [name for name in preferred if name in fieldnames]
    if not columns:
        columns = list(fieldnames)

    with (output_dir / "failed_runs.csv").open(
        "w", encoding="utf-8", newline=""
    ) as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, extrasaction="ignore")
        writer.writeheader()
        for row in failed:
            writer.writerow(row)


def write_summary(output_dir, rows, failed, rows_by_variant, image_files, validation):
    object_counts = sorted({row["object_count"] for row in rows})
    variants = sorted(rows_by_variant)
    lines = [
        "Geometry Time Orthogonal Plot Generation Summary",
        "================================================",
        f"Successful rows used: {len(rows)}",
        f"Non-success rows: {len(failed)}",
        f"Object counts: {object_counts}",
        f"Variants: {variants}",
        "Other measured dimensions are fixed: material_count=1, geometry_count=1, overdraw_count=0.",
        f"Logical graphs generated: {len(image_files) // 2}",
        f"PNG/SVG files generated: {len(image_files)}",
        "",
        "Graph set:",
        "- 01: average pass time, log X / log Y",
        "- 02: average pass time, linear X / linear Y",
        "- 03: average pass time per object, log X / log Y",
        "- 04: total time relative to Forward, log X / linear Y",
        "",
        "Validation:",
    ]
    for row in validation:
        lines.append(
            f"- Variant {row['variant']} · {row['renderer']}: "
            f"max total-minus-pass-sum error = {row['max_total_error_ms']:.12g} ms "
            f"[{row['status']}]"
        )
    lines.extend(
        [
            "",
            "Notes:",
            "- All variant totals and all active individual passes are included in every graph.",
            "- Variant 1 has one forward pass, so its forward line exactly overlaps the total line.",
            "- No condition-specific graph split is needed because object_count is the only swept non-variant dimension.",
        ]
    )
    (output_dir / "run_summary.txt").write_text("\n".join(lines), encoding="utf-8")


def create_zip(output_dir, csv_path):
    zip_path = output_dir.parent / f"{output_dir.name}_bundle.zip"
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(output_dir.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(output_dir.parent))
        archive.write(csv_path, csv_path.name)
    return zip_path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path", type=Path)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("geometry_time_orthogonal_plots"),
    )
    args = parser.parse_args()

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    rows, failed, fieldnames = load_csv(args.csv_path)
    rows_by_variant = aggregate(rows)
    pass_map = active_passes(rows_by_variant)

    validation = validate_totals(rows_by_variant, output_dir)
    export_failed_runs(failed, fieldnames, output_dir)

    image_files = []
    for spec in PLOT_FORMATS:
        image_files.extend(
            plot_all_variants(rows_by_variant, pass_map, spec, output_dir)
        )
    image_files.extend(
        plot_total_relative_to_forward(rows_by_variant, output_dir)
    )

    write_summary(
        output_dir,
        rows,
        failed,
        rows_by_variant,
        image_files,
        validation,
    )
    zip_path = create_zip(output_dir, args.csv_path)

    print(f"Logical graphs generated: {len(image_files) // 2}")
    print(f"PNG/SVG files generated: {len(image_files)}")
    print(f"Output directory: {output_dir}")
    print(f"Bundle ZIP: {zip_path}")


if __name__ == "__main__":
    main()
