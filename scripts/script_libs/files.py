from __future__ import annotations

import csv
import json
import shutil
import sys
from datetime import datetime
from pathlib import Path
from typing import Any

from .common import fail, render_argument, trim_error
from .models import PROGRAM_RESULT_FIELDS


def result_paths(spec_path: Path, _spec: dict[str, Any] | None) -> tuple[Path, Path, Path, Path]:
    output_name = f"{spec_path.stem}.csv"

    output_stem = Path(output_name).stem
    output_dir = spec_path.parent / "results" / output_stem
    return (
        output_dir,
        output_dir / output_name,
        output_dir / spec_path.name,
        output_dir / f"{output_stem}_run_report.json",
    )


def copy_if_possible(source: Path, destination: Path) -> str:
    try:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        return ""
    except Exception as error:
        return trim_error(error)


def read_result_rows(path: Path) -> list[dict[str, str]]:
    if not path.exists() or path.stat().st_size == 0:
        return []
    with path.open("r", encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        if not reader.fieldnames:
            fail(f"Benchmark output CSV has no header: {path}")
        missing_fields = [
            field_name
            for field_name in PROGRAM_RESULT_FIELDS
            if field_name not in reader.fieldnames
        ]
        if missing_fields:
            fail(
                "Benchmark output CSV is missing ProgramResult fields: "
                + ", ".join(missing_fields)
            )
        rows: list[dict[str, str]] = []
        for row in reader:
            normalized = {
                str(key): "" if value is None else str(value)
                for key, value in row.items()
                if key is not None
            }
            if any(value != "" for value in normalized.values()):
                rows.append(normalized)
        return rows


def append_rows_flexible(path: Path, rows: list[dict[str, Any]]) -> None:
    if not rows:
        return

    path.parent.mkdir(parents=True, exist_ok=True)
    string_rows = [{str(key): render_argument(value) for key, value in row.items()} for row in rows]
    existing_rows: list[dict[str, str]] = []
    existing_fieldnames: list[str] = []

    if path.exists() and path.stat().st_size > 0:
        try:
            with path.open("r", encoding="utf-8-sig", newline="") as file:
                reader = csv.DictReader(file)
                existing_fieldnames = list(reader.fieldnames or [])
                existing_rows = [dict(row) for row in reader]
        except Exception as error:
            stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            backup = path.with_name(f"{path.stem}.corrupt_{stamp}{path.suffix}")
            shutil.move(path, backup)
            print(
                f"WARNING: Existing CSV could not be read and was moved to {backup}: {error}",
                file=sys.stderr,
            )

    fieldnames = list(existing_fieldnames)
    for row in string_rows:
        for key in row:
            if key not in fieldnames:
                fieldnames.append(key)

    schema_changed = fieldnames != existing_fieldnames
    file_is_empty = not path.exists() or path.stat().st_size == 0
    if file_is_empty or schema_changed:
        temporary_path = path.with_suffix(path.suffix + ".tmp")
        with temporary_path.open("w", encoding="utf-8-sig", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=fieldnames, extrasaction="ignore")
            writer.writeheader()
            writer.writerows(existing_rows)
            writer.writerows(string_rows)
        temporary_path.replace(path)
        return

    with path.open("a", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames, extrasaction="ignore")
        writer.writerows(string_rows)


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = path.with_suffix(path.suffix + ".tmp")
    with temporary_path.open("w", encoding="utf-8") as file:
        json.dump(value, file, ensure_ascii=False, indent=2)
        file.write("\n")
    temporary_path.replace(path)


def safe_write_json(path: Path, value: dict[str, Any]) -> bool:
    try:
        write_json_atomic(path, value)
        return True
    except Exception as error:
        print(f"WARNING: Could not write JSON report {path}: {error}", file=sys.stderr)
        return False
