#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path

FILE_PATTERN = "run_*.csv_*_result.csv"
NAME_PATTERN = re.compile(r"^run_(?P<run_id>\d+)\.csv_(?P<result_id>\d+)_result\.csv$")


def sort_key(path: Path) -> tuple[int, int, str]:
    match = NAME_PATTERN.match(path.name)
    if match:
        return int(match.group("run_id")), int(match.group("result_id")), path.name
    return 10**18, 10**18, path.name


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path, default=Path("merged_results.csv"))
    args = parser.parse_args()

    directory = args.directory.resolve()
    output = args.output if args.output.is_absolute() else directory / args.output
    files = sorted((p for p in directory.glob(FILE_PATTERN) if p.is_file()), key=sort_key)
    if not files:
        print(f"[오류] {FILE_PATTERN} 파일이 없습니다.")
        return 1

    metadata = ["source_file", "run_id", "result_id"]
    data_columns: list[str] = []
    rows: list[dict[str, str]] = []

    for path in files:
        with path.open("r", encoding="utf-8-sig", newline="") as file:
            reader = csv.DictReader(file)
            if reader.fieldnames is None:
                raise ValueError(f"헤더가 없습니다: {path}")
            current = list(reader)
            for column in reader.fieldnames:
                if column not in metadata and column not in data_columns:
                    data_columns.append(column)

        match = NAME_PATTERN.match(path.name)
        run_id = match.group("run_id") if match else ""
        result_id = match.group("result_id") if match else ""
        for row in current:
            rows.append({"source_file": path.name, "run_id": run_id, "result_id": result_id, **row})
        print(f"[읽음] {path.name}: {len(current)}행")

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8-sig", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=metadata + data_columns, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)

    print(f"[완료] {len(files)}개 파일, {len(rows)}행 -> {output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
