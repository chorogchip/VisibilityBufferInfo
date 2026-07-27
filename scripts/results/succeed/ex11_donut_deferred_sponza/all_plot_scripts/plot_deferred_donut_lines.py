#!/usr/bin/env python3
"""
Benchmark renderer와 Donut renderer의 Deferred 계열 total GPU time을
ALU별 프레임 꺾은선 그래프로 비교한다.

표시하는 선:
- Benchmark Deferred
- Benchmark Deferred+Prepass
- Benchmark VisBuf+GBuffer
- Donut Deferred
- Donut Deferred+Prepass

scene JSON sweep 순서:
    renderer_variant: 바깥 루프
    alu_calc_count:   안쪽 루프

사용 예:
    python plot_deferred_donut_lines.py \
        "merged_results(4).csv" \
        donut.csv \
        "run_00000.csv_0_result(1).csv" \
        scene_geom_ex.json \
        --output-dir deferred_donut_line_plots \
        --zip-output deferred_donut_line_plots.zip
"""

from __future__ import annotations

import argparse
import csv
import json
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
    6: "VisBuf+GBuffer",
}

TARGET_VARIANTS = (
    "Deferred",
    "Deferred+Prepass",
    "VisBuf+GBuffer",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Deferred 계열과 Donut 결과를 ALU별 꺾은선 그래프로 비교합니다."
    )
    parser.add_argument("merged_csv", type=Path)
    parser.add_argument("donut_deferred_csv", type=Path)
    parser.add_argument("donut_prepass_csv", type=Path)
    parser.add_argument("scene_json", type=Path)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("deferred_donut_line_plots"),
    )
    parser.add_argument(
        "--zip-output",
        type=Path,
        default=Path("deferred_donut_line_plots.zip"),
    )
    parser.add_argument(
        "--scene-name",
        default="Sponza",
    )
    return parser.parse_args()


def read_rows(path: Path) -> list[dict[str, str]]:
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


def load_total_series(path: Path) -> dict[int, float]:
    rows = read_rows(path)
    if not rows:
        raise ValueError(f"CSV가 비어 있습니다: {path}")

    required = {"frame", "total"}
    missing = required - set(rows[0])
    if missing:
        raise ValueError(
            f"{path.name} 필수 열 누락: {', '.join(sorted(missing))}"
        )

    return {
        int(row["frame"]): float(row["total"])
        for row in rows
    }


def load_benchmark_series(
    path: Path,
    mapping: dict[int, tuple[int, int]],
) -> dict[tuple[str, int], dict[int, float]]:
    rows = read_rows(path)
    if not rows:
        raise ValueError("merged CSV가 비어 있습니다.")

    required = {"run_id", "frame", "total"}
    missing = required - set(rows[0])
    if missing:
        raise ValueError(
            f"merged CSV 필수 열 누락: {', '.join(sorted(missing))}"
        )

    series: dict[tuple[str, int], dict[int, float]] = defaultdict(dict)

    for row in rows:
        run_id = int(row["run_id"])
        if run_id not in mapping:
            continue

        variant_id, alu = mapping[run_id]
        variant_name = VARIANT_NAMES.get(variant_id)

        if variant_name not in TARGET_VARIANTS:
            continue

        series[(variant_name, alu)][int(row["frame"])] = float(row["total"])

    return series


def create_plots(
    benchmark: dict[tuple[str, int], dict[int, float]],
    donut_deferred: dict[int, float],
    donut_prepass: dict[int, float],
    alu_values: list[int],
    output_dir: Path,
    scene_name: str,
) -> list[Path]:
    required_keys = {
        (variant, alu)
        for variant in TARGET_VARIANTS
        for alu in alu_values
    }
    missing_keys = required_keys - set(benchmark)
    if missing_keys:
        raise ValueError(f"필요한 benchmark run 누락: {sorted(missing_keys)}")

    all_frame_sets = [
        set(donut_deferred),
        set(donut_prepass),
    ]
    all_frame_sets.extend(set(benchmark[key]) for key in sorted(required_keys))
    common_frames = sorted(set.intersection(*all_frame_sets))

    if not common_frames:
        raise ValueError("모든 데이터에 공통으로 존재하는 frame이 없습니다.")

    output_dir.mkdir(parents=True, exist_ok=True)
    created: list[Path] = []

    for alu in alu_values:
        figure = plt.figure(figsize=(13, 6.5))

        for variant in TARGET_VARIANTS:
            values = [
                benchmark[(variant, alu)][frame]
                for frame in common_frames
            ]
            plt.plot(
                common_frames,
                values,
                linewidth=1.25,
                label=f"Benchmark {variant} — ALU {alu}",
            )

        plt.plot(
            common_frames,
            [donut_deferred[frame] for frame in common_frames],
            linewidth=1.25,
            label="Donut Deferred",
        )
        plt.plot(
            common_frames,
            [donut_prepass[frame] for frame in common_frames],
            linewidth=1.25,
            label="Donut Deferred+Prepass",
        )

        plt.title(
            f"{scene_name}: Deferred renderer total GPU time — ALU {alu}"
        )
        plt.xlabel("Profiled frame")
        plt.ylabel("Total GPU time (ms)")
        plt.grid(True, alpha=0.3)
        plt.legend(ncol=2)
        plt.tight_layout()

        output_path = output_dir / f"deferred_lines_alu_{alu}.png"
        figure.savefig(output_path, dpi=180, bbox_inches="tight")
        plt.close(figure)
        created.append(output_path)

    print(
        f"Common frames: {common_frames[0]}..{common_frames[-1]} "
        f"({len(common_frames)} rows)"
    )
    return created


def create_zip(
    plot_files: list[Path],
    script_path: Path,
    zip_path: Path,
) -> None:
    if zip_path.exists():
        zip_path.unlink()

    with zipfile.ZipFile(
        zip_path,
        mode="w",
        compression=zipfile.ZIP_DEFLATED,
    ) as archive:
        for plot_file in plot_files:
            archive.write(plot_file, arcname=plot_file.name)
        archive.write(script_path, arcname=script_path.name)


def main() -> int:
    args = parse_args()

    variants, alu_values = read_sweep(args.scene_json)
    mapping = build_run_mapping(variants, alu_values)

    benchmark = load_benchmark_series(args.merged_csv, mapping)
    donut_deferred = load_total_series(args.donut_deferred_csv)
    donut_prepass = load_total_series(args.donut_prepass_csv)

    plots = create_plots(
        benchmark=benchmark,
        donut_deferred=donut_deferred,
        donut_prepass=donut_prepass,
        alu_values=alu_values,
        output_dir=args.output_dir,
        scene_name=args.scene_name,
    )
    create_zip(plots, Path(__file__).resolve(), args.zip_output)

    print("Selected benchmark runs:")
    for run_id, (variant_id, alu) in mapping.items():
        variant_name = VARIANT_NAMES.get(variant_id)
        if variant_name in TARGET_VARIANTS:
            print(f"  run_id {run_id:02d}: {variant_name}, ALU {alu}")

    print(f"Created {len(plots)} line plots")
    print(f"ZIP: {args.zip_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
