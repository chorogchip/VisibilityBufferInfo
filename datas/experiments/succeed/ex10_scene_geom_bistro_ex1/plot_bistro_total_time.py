#!/usr/bin/env python3
"""
Bistro 실험 결과 CSV에서 ALU별 total GPU time 꺾은선 그래프를 생성한다.

실험 sweep 순서:
    renderer_variant: [1, 2, 3, 4, 5, 6]
    alu_calc_count:   [1, 5, 10, 20]

즉 renderer_variant가 바깥 루프, ALU가 안쪽 루프다.

사용 예:
    python plot_bistro_total_time.py merged_results.csv

    python plot_bistro_total_time.py merged_results.csv \
        --output-dir bistro_total_time_plots \
        --zip-output bistro_total_time_plots.zip
"""

from __future__ import annotations

import argparse
import shutil
import zipfile
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


ALU_VALUES = [1, 5, 10, 20]

VARIANT_NAMES = {
    1: "Forward",
    2: "Forward+Prepass",
    3: "Deferred",
    4: "TVB",
    5: "Deferred+Prepass",
    6: "TVB+GBuffer",
}

VARIANT_ORDER = [VARIANT_NAMES[index] for index in range(1, 7)]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Bistro renderer 실험의 ALU별 total GPU time 그래프를 생성합니다."
    )
    parser.add_argument(
        "input_csv",
        type=Path,
        help="merge_result_csvs.py로 합친 결과 CSV",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("bistro_total_time_plots"),
        help="PNG 출력 디렉터리",
    )
    parser.add_argument(
        "--zip-output",
        type=Path,
        default=Path("bistro_total_time_plots.zip"),
        help="생성된 PNG 4개를 담을 ZIP 경로",
    )
    parser.add_argument(
        "--scene-name",
        default="Bistro Exterior",
        help="그래프 제목에 표시할 scene 이름",
    )
    return parser.parse_args()


def validate_input(data: pd.DataFrame) -> None:
    required_columns = {"run_id", "frame", "total"}
    missing_columns = required_columns - set(data.columns)

    if missing_columns:
        missing_text = ", ".join(sorted(missing_columns))
        raise ValueError(f"필수 CSV 열이 없습니다: {missing_text}")

    expected_run_count = len(VARIANT_NAMES) * len(ALU_VALUES)
    actual_run_count = data["run_id"].nunique()

    if actual_run_count != expected_run_count:
        print(
            f"[경고] 예상 run 수는 {expected_run_count}개지만 "
            f"CSV에는 {actual_run_count}개가 있습니다."
        )


def add_experiment_columns(data: pd.DataFrame) -> pd.DataFrame:
    result = data.copy()
    alu_count = len(ALU_VALUES)

    # Sweep 순서:
    # renderer_variant 바깥 루프, alu_calc_count 안쪽 루프
    result["renderer_variant"] = result["run_id"] // alu_count + 1
    result["alu_calc_count"] = result["run_id"].map(
        lambda run_id: ALU_VALUES[int(run_id) % alu_count]
    )
    result["variant"] = result["renderer_variant"].map(VARIANT_NAMES)

    unknown_variants = result[result["variant"].isna()]["renderer_variant"].unique()
    if len(unknown_variants) > 0:
        values = ", ".join(map(str, sorted(unknown_variants)))
        raise ValueError(f"알 수 없는 renderer variant가 있습니다: {values}")

    return result


def create_plots(
    data: pd.DataFrame,
    output_dir: Path,
    scene_name: str,
) -> list[Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    created_files: list[Path] = []

    for alu in ALU_VALUES:
        figure = plt.figure(figsize=(13, 6.5))

        for variant in VARIANT_ORDER:
            run = data[
                (data["alu_calc_count"] == alu)
                & (data["variant"] == variant)
            ].sort_values("frame")

            if run.empty:
                print(f"[경고] ALU {alu}, {variant} 데이터가 없습니다.")
                continue

            plt.plot(
                run["frame"],
                run["total"],
                label=variant,
                linewidth=1.35,
            )

        plt.title(f"{scene_name}: Total GPU Time by Frame — ALU {alu}")
        plt.xlabel("Frame")
        plt.ylabel("GPU time (ms)")
        plt.grid(True, alpha=0.3)
        plt.legend(ncol=2)
        plt.tight_layout()

        output_path = output_dir / f"bistro_total_time_alu_{alu}.png"
        figure.savefig(output_path, dpi=180, bbox_inches="tight")
        plt.close(figure)

        created_files.append(output_path)
        print(f"[생성] {output_path}")

    return created_files


def create_zip(files: list[Path], zip_path: Path) -> None:
    zip_path.parent.mkdir(parents=True, exist_ok=True)

    if zip_path.exists():
        zip_path.unlink()

    with zipfile.ZipFile(
        zip_path,
        mode="w",
        compression=zipfile.ZIP_DEFLATED,
    ) as archive:
        for file_path in files:
            archive.write(file_path, arcname=file_path.name)

    print(f"[생성] {zip_path}")


def main() -> int:
    args = parse_args()

    input_csv = args.input_csv.resolve()
    output_dir = args.output_dir.resolve()
    zip_output = args.zip_output.resolve()

    if not input_csv.exists():
        print(f"[오류] 입력 CSV를 찾을 수 없습니다: {input_csv}")
        return 1

    try:
        data = pd.read_csv(input_csv)
        validate_input(data)
        data = add_experiment_columns(data)

        if output_dir.exists():
            shutil.rmtree(output_dir)

        created_files = create_plots(
            data=data,
            output_dir=output_dir,
            scene_name=args.scene_name,
        )
        create_zip(created_files, zip_output)

    except (OSError, ValueError, KeyError, pd.errors.ParserError) as error:
        print(f"[오류] {error}")
        return 1

    print(f"[완료] 그래프 {len(created_files)}개")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
