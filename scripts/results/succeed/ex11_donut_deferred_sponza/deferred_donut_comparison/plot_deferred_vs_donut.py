#!/usr/bin/env python3
"""
Benchmark renderer의 Deferred 계열과 Donut Deferred 계열을 비교한다.

scene_geom_ex.json의 sweep 순서:
    renderer_variant: [1, 2, 3, 4, 5, 6]  # 바깥 루프
    alu_calc_count:   [10, 20, 40, 80, 160]  # 안쪽 루프

따라서 기본 설정에서는:
    run_id 10~14: Deferred
    run_id 20~24: Deferred+Prepass

Donut Deferred CSV는 프레임 시계열로 비교한다.
Donut Deferred+Prepass CSV가 aggregate summary 1행인 경우에는
프레임 그래프에 평균 수평선으로 표시하고, 요약 그래프에는 평균/중앙값을 사용한다.

사용 예:
    python plot_deferred_vs_donut.py \
        "merged_results(4).csv" \
        donut.csv \
        donut_deferred_sponza_prepass.csv \
        scene_geom_ex.json \
        --output-dir deferred_donut_comparison \
        --zip-output deferred_donut_comparison.zip
"""

from __future__ import annotations

import argparse
import csv
import json
import statistics
import zipfile
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt


VARIANT_NAMES = {
    1: "Forward",
    2: "Forward+Prepass",
    3: "Deferred",
    4: "TVB",
    5: "Deferred+Prepass",
    6: "TVB+GBuffer",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Benchmark Deferred/Deferred+Prepass와 Donut Deferred 계열을 비교합니다."
    )
    parser.add_argument("merged_csv", type=Path)
    parser.add_argument("donut_csv", type=Path)
    parser.add_argument("donut_prepass_csv", type=Path)
    parser.add_argument("scene_json", type=Path)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("deferred_donut_comparison"),
    )
    parser.add_argument(
        "--zip-output",
        type=Path,
        default=Path("deferred_donut_comparison.zip"),
    )
    parser.add_argument(
        "--scene-name",
        default="Sponza",
    )
    return parser.parse_args()


def read_csv_rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def read_sweep(scene_json: Path) -> tuple[list[int], list[int]]:
    config = json.loads(scene_json.read_text(encoding="utf-8"))
    sweep = config["sweep"]
    variants = [int(value) for value in sweep["renderer_variant"]]
    alu_values = [int(value) for value in sweep["alu_calc_count"]]
    return variants, alu_values


def build_run_mapping(
    variants: list[int],
    alu_values: list[int],
) -> dict[int, tuple[int, int]]:
    mapping: dict[int, tuple[int, int]] = {}
    run_id = 0
    for variant in variants:
        for alu in alu_values:
            mapping[run_id] = (variant, alu)
            run_id += 1
    return mapping


def load_merged_series(
    merged_csv: Path,
    mapping: dict[int, tuple[int, int]],
) -> dict[tuple[str, int], dict[int, float]]:
    required = {"run_id", "frame", "total"}
    series: dict[tuple[str, int], dict[int, float]] = defaultdict(dict)

    rows = read_csv_rows(merged_csv)
    if not rows:
        raise ValueError("merged CSV가 비어 있습니다.")

    missing = required - set(rows[0])
    if missing:
        raise ValueError(f"merged CSV 필수 열 누락: {', '.join(sorted(missing))}")

    for row in rows:
        run_id = int(row["run_id"])
        if run_id not in mapping:
            continue

        variant_id, alu = mapping[run_id]
        variant_name = VARIANT_NAMES.get(variant_id)
        if variant_name not in {"Deferred", "Deferred+Prepass"}:
            continue

        series[(variant_name, alu)][int(row["frame"])] = float(row["total"])

    return series


def load_donut_series(donut_csv: Path) -> dict[int, float]:
    rows = read_csv_rows(donut_csv)
    if not rows:
        raise ValueError("Donut CSV가 비어 있습니다.")

    required = {"frame", "total"}
    missing = required - set(rows[0])
    if missing:
        raise ValueError(f"Donut CSV 필수 열 누락: {', '.join(sorted(missing))}")

    return {int(row["frame"]): float(row["total"]) for row in rows}


def find_pass_average(row: dict[str, str], pass_name: str) -> float | None:
    for index in range(32):
        if row.get(f"pass_name_{index}", "").strip() == pass_name:
            value = row.get(f"pass_{index}_time_avg_ms", "").strip()
            return float(value) if value else None
    return None


def load_donut_prepass_summary(path: Path) -> dict[str, float]:
    rows = read_csv_rows(path)
    if not rows:
        raise ValueError("Donut prepass CSV가 비어 있습니다.")

    row = rows[0]
    result = {
        "avg": float(row["total_time_avg_ms"]),
        "median": float(row["total_time_median_ms"]),
        "min": float(row["total_time_min_ms"]),
        "max": float(row["total_time_max_ms"]),
        "p10": float(row["total_time_p10_ms"]),
        "p90": float(row["total_time_p90_ms"]),
    }

    for pass_name in ("depth_prepass", "geometry", "lighting", "tonemap"):
        value = find_pass_average(row, pass_name)
        if value is not None:
            result[f"{pass_name}_avg"] = value

    return result


def values_for_frames(series: dict[int, float], frames: list[int]) -> list[float]:
    return [series[frame] for frame in frames]


def mean(values: list[float]) -> float:
    return statistics.fmean(values)


def median(values: list[float]) -> float:
    return statistics.median(values)


def write_summary_csv(
    path: Path,
    alu_values: list[int],
    benchmark: dict[tuple[str, int], dict[int, float]],
    donut: dict[int, float],
    donut_prepass: dict[str, float],
    common_frames: list[int],
) -> None:
    donut_values = values_for_frames(donut, common_frames)
    donut_avg = mean(donut_values)
    donut_median = median(donut_values)

    fields = [
        "alu_calc_count",
        "benchmark_deferred_avg_ms",
        "benchmark_deferred_median_ms",
        "benchmark_deferred_prepass_avg_ms",
        "benchmark_deferred_prepass_median_ms",
        "donut_deferred_avg_ms_common_frames",
        "donut_deferred_median_ms_common_frames",
        "donut_deferred_prepass_avg_ms",
        "donut_deferred_prepass_median_ms",
        "benchmark_deferred_over_donut_ratio_avg",
        "benchmark_prepass_over_donut_prepass_ratio_avg",
    ]

    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()

        for alu in alu_values:
            deferred_values = values_for_frames(
                benchmark[("Deferred", alu)], common_frames
            )
            prepass_values = values_for_frames(
                benchmark[("Deferred+Prepass", alu)], common_frames
            )
            deferred_avg = mean(deferred_values)
            prepass_avg = mean(prepass_values)

            writer.writerow(
                {
                    "alu_calc_count": alu,
                    "benchmark_deferred_avg_ms": f"{deferred_avg:.6f}",
                    "benchmark_deferred_median_ms": f"{median(deferred_values):.6f}",
                    "benchmark_deferred_prepass_avg_ms": f"{prepass_avg:.6f}",
                    "benchmark_deferred_prepass_median_ms": f"{median(prepass_values):.6f}",
                    "donut_deferred_avg_ms_common_frames": f"{donut_avg:.6f}",
                    "donut_deferred_median_ms_common_frames": f"{donut_median:.6f}",
                    "donut_deferred_prepass_avg_ms": f"{donut_prepass['avg']:.6f}",
                    "donut_deferred_prepass_median_ms": f"{donut_prepass['median']:.6f}",
                    "benchmark_deferred_over_donut_ratio_avg": f"{deferred_avg / donut_avg:.6f}",
                    "benchmark_prepass_over_donut_prepass_ratio_avg": f"{prepass_avg / donut_prepass['avg']:.6f}",
                }
            )


def create_frame_plots(
    output_dir: Path,
    scene_name: str,
    alu_values: list[int],
    benchmark: dict[tuple[str, int], dict[int, float]],
    donut: dict[int, float],
    donut_prepass: dict[str, float],
    common_frames: list[int],
) -> list[Path]:
    created: list[Path] = []
    donut_values = values_for_frames(donut, common_frames)

    for alu in alu_values:
        deferred_values = values_for_frames(
            benchmark[("Deferred", alu)], common_frames
        )
        prepass_values = values_for_frames(
            benchmark[("Deferred+Prepass", alu)], common_frames
        )

        figure = plt.figure(figsize=(13, 6.5))
        plt.plot(
            common_frames,
            deferred_values,
            linewidth=1.25,
            label=f"Benchmark Deferred — ALU {alu}",
        )
        plt.plot(
            common_frames,
            prepass_values,
            linewidth=1.25,
            label=f"Benchmark Deferred+Prepass — ALU {alu}",
        )
        plt.plot(
            common_frames,
            donut_values,
            linewidth=1.25,
            label="Donut Deferred",
        )
        plt.axhline(
            donut_prepass["avg"],
            linestyle="--",
            linewidth=1.25,
            label="Donut Deferred+Prepass — aggregate avg",
        )

        plt.title(f"{scene_name}: Deferred total GPU time by frame — ALU {alu}")
        plt.xlabel("Profiled frame")
        plt.ylabel("Total GPU time (ms)")
        plt.grid(True, alpha=0.3)
        plt.legend(ncol=2)
        plt.tight_layout()

        output_path = output_dir / f"deferred_vs_donut_alu_{alu}.png"
        figure.savefig(output_path, dpi=180, bbox_inches="tight")
        plt.close(figure)
        created.append(output_path)

    return created


def create_summary_plot(
    output_dir: Path,
    scene_name: str,
    alu_values: list[int],
    benchmark: dict[tuple[str, int], dict[int, float]],
    donut: dict[int, float],
    donut_prepass: dict[str, float],
    common_frames: list[int],
    statistic_name: str,
) -> Path:
    reducer = mean if statistic_name == "Average" else median
    x = list(range(len(alu_values)))

    benchmark_deferred = []
    benchmark_prepass = []
    for alu in alu_values:
        benchmark_deferred.append(
            reducer(values_for_frames(benchmark[("Deferred", alu)], common_frames))
        )
        benchmark_prepass.append(
            reducer(
                values_for_frames(
                    benchmark[("Deferred+Prepass", alu)], common_frames
                )
            )
        )

    donut_value = reducer(values_for_frames(donut, common_frames))
    donut_prepass_value = (
        donut_prepass["avg"]
        if statistic_name == "Average"
        else donut_prepass["median"]
    )

    figure = plt.figure(figsize=(10.5, 6.5))
    plt.plot(x, benchmark_deferred, marker="o", label="Benchmark Deferred")
    plt.plot(x, benchmark_prepass, marker="o", label="Benchmark Deferred+Prepass")
    plt.plot(x, [donut_value] * len(x), marker="o", label="Donut Deferred")
    plt.plot(
        x,
        [donut_prepass_value] * len(x),
        marker="o",
        linestyle="--",
        label="Donut Deferred+Prepass",
    )

    plt.xticks(x, [str(value) for value in alu_values])
    plt.title(f"{scene_name}: {statistic_name.lower()} total GPU time vs ALU")
    plt.xlabel("ALU calculation count")
    plt.ylabel(f"{statistic_name} total GPU time (ms)")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    output_path = output_dir / f"deferred_vs_donut_{statistic_name.lower()}.png"
    figure.savefig(output_path, dpi=180, bbox_inches="tight")
    plt.close(figure)
    return output_path


def create_ratio_plot(
    output_dir: Path,
    scene_name: str,
    alu_values: list[int],
    benchmark: dict[tuple[str, int], dict[int, float]],
    donut: dict[int, float],
    donut_prepass: dict[str, float],
    common_frames: list[int],
) -> Path:
    x = list(range(len(alu_values)))
    donut_avg = mean(values_for_frames(donut, common_frames))

    deferred_ratio = []
    prepass_ratio = []
    for alu in alu_values:
        deferred_ratio.append(
            mean(values_for_frames(benchmark[("Deferred", alu)], common_frames))
            / donut_avg
        )
        prepass_ratio.append(
            mean(
                values_for_frames(
                    benchmark[("Deferred+Prepass", alu)], common_frames
                )
            )
            / donut_prepass["avg"]
        )

    width = 0.36
    figure = plt.figure(figsize=(10.5, 6.5))
    plt.bar(
        [value - width / 2 for value in x],
        deferred_ratio,
        width=width,
        label="Benchmark Deferred / Donut Deferred",
    )
    plt.bar(
        [value + width / 2 for value in x],
        prepass_ratio,
        width=width,
        label="Benchmark Prepass / Donut Prepass",
    )
    plt.axhline(1.0, linestyle="--", linewidth=1.2)
    plt.xticks(x, [str(value) for value in alu_values])
    plt.title(f"{scene_name}: average time ratio to Donut baseline")
    plt.xlabel("ALU calculation count")
    plt.ylabel("Time ratio (below 1 = benchmark faster)")
    plt.grid(True, axis="y", alpha=0.3)
    plt.legend()
    plt.tight_layout()

    output_path = output_dir / "deferred_vs_donut_average_ratio.png"
    figure.savefig(output_path, dpi=180, bbox_inches="tight")
    plt.close(figure)
    return output_path


def create_zip(files: list[Path], zip_path: Path) -> None:
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(
        zip_path,
        mode="w",
        compression=zipfile.ZIP_DEFLATED,
    ) as archive:
        for path in files:
            archive.write(path, arcname=path.name)


def main() -> int:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    variants, alu_values = read_sweep(args.scene_json)
    mapping = build_run_mapping(variants, alu_values)

    benchmark = load_merged_series(args.merged_csv, mapping)
    donut = load_donut_series(args.donut_csv)
    donut_prepass = load_donut_prepass_summary(args.donut_prepass_csv)

    required_keys = {
        (variant, alu)
        for variant in ("Deferred", "Deferred+Prepass")
        for alu in alu_values
    }
    missing_keys = required_keys - set(benchmark)
    if missing_keys:
        raise ValueError(f"필요한 benchmark run 누락: {sorted(missing_keys)}")

    common_frames = sorted(
        set(donut)
        .intersection(*(set(benchmark[key]) for key in sorted(required_keys)))
    )
    if not common_frames:
        raise ValueError("공통 frame이 없습니다.")

    created = create_frame_plots(
        args.output_dir,
        args.scene_name,
        alu_values,
        benchmark,
        donut,
        donut_prepass,
        common_frames,
    )
    created.append(
        create_summary_plot(
            args.output_dir,
            args.scene_name,
            alu_values,
            benchmark,
            donut,
            donut_prepass,
            common_frames,
            "Average",
        )
    )
    created.append(
        create_summary_plot(
            args.output_dir,
            args.scene_name,
            alu_values,
            benchmark,
            donut,
            donut_prepass,
            common_frames,
            "Median",
        )
    )
    created.append(
        create_ratio_plot(
            args.output_dir,
            args.scene_name,
            alu_values,
            benchmark,
            donut,
            donut_prepass,
            common_frames,
        )
    )

    summary_path = args.output_dir / "deferred_donut_summary.csv"
    write_summary_csv(
        summary_path,
        alu_values,
        benchmark,
        donut,
        donut_prepass,
        common_frames,
    )
    created.append(summary_path)

    create_zip(created, args.zip_output)

    print(f"Common frames: {common_frames[0]}..{common_frames[-1]} ({len(common_frames)} rows)")
    print("Run mapping:")
    for run_id, (variant, alu) in mapping.items():
        if variant in (3, 5):
            print(f"  run_id {run_id:02d}: {VARIANT_NAMES[variant]}, ALU {alu}")
    print(f"Created {len(created)} files in {args.output_dir}")
    print(f"ZIP: {args.zip_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
