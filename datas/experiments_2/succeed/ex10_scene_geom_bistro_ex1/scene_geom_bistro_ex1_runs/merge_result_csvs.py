#!/usr/bin/env python3
"""
현재 디렉터리의 run_*.csv_*_result.csv 파일을 하나의 CSV로 합친다.

사용 예:
    python merge_result_csvs.py
    python merge_result_csvs.py --directory . --output merged_results.csv
"""

from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path
from typing import Iterable


FILE_PATTERN = "run_*.csv_*_result.csv"
NAME_PATTERN = re.compile(
    r"^run_(?P<run_id>\d+)\.csv_(?P<result_id>\d+)_result\.csv$"
)


def sort_key(path: Path) -> tuple[int, int, str]:
    match = NAME_PATTERN.match(path.name)
    if match:
        return (
            int(match.group("run_id")),
            int(match.group("result_id")),
            path.name,
        )
    return (10**18, 10**18, path.name)


def collect_files(directory: Path) -> list[Path]:
    return sorted(
        (path for path in directory.glob(FILE_PATTERN) if path.is_file()),
        key=sort_key,
    )


def read_rows(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    # Excel과 Python 양쪽에서 다루기 편하도록 UTF-8 BOM도 허용한다.
    with path.open("r", encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)

        if reader.fieldnames is None:
            raise ValueError(f"헤더가 없는 CSV입니다: {path}")

        return list(reader.fieldnames), list(reader)


def merge_csvs(files: Iterable[Path], output_path: Path) -> tuple[int, int, list[str]]:
    files = list(files)
    if not files:
        raise FileNotFoundError(
            f"'{FILE_PATTERN}' 패턴에 맞는 파일이 없습니다."
        )

    metadata_columns = ["source_file", "run_id", "result_id"]
    data_columns: list[str] = []
    rows_to_write: list[dict[str, str]] = []

    for path in files:
        columns, rows = read_rows(path)

        for column in columns:
            if column not in data_columns and column not in metadata_columns:
                data_columns.append(column)

        match = NAME_PATTERN.match(path.name)
        run_id = match.group("run_id") if match else ""
        result_id = match.group("result_id") if match else ""

        for row in rows:
            rows_to_write.append(
                {
                    "source_file": path.name,
                    "run_id": run_id,
                    "result_id": result_id,
                    **row,
                }
            )

        print(f"[읽음] {path.name}: {len(rows)}행")

    fieldnames = metadata_columns + data_columns

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8-sig", newline="") as file:
        writer = csv.DictWriter(
            file,
            fieldnames=fieldnames,
            extrasaction="ignore",
        )
        writer.writeheader()
        writer.writerows(rows_to_write)

    return len(files), len(rows_to_write), fieldnames


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="run_*.csv_*_result.csv 파일들을 하나의 CSV로 합칩니다."
    )
    parser.add_argument(
        "--directory",
        type=Path,
        default=Path.cwd(),
        help="입력 CSV 디렉터리. 기본값: 현재 디렉터리",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("merged_results.csv"),
        help="출력 CSV 경로. 기본값: merged_results.csv",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    directory = args.directory.resolve()

    output_path = args.output
    if not output_path.is_absolute():
        output_path = directory / output_path
    output_path = output_path.resolve()

    files = collect_files(directory)

    try:
        file_count, row_count, columns = merge_csvs(files, output_path)
    except (OSError, ValueError) as error:
        print(f"[오류] {error}")
        return 1

    print()
    print(f"[완료] 입력 파일: {file_count}개")
    print(f"[완료] 전체 행: {row_count}개")
    print(f"[완료] 열 수: {len(columns)}개")
    print(f"[완료] 출력: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
