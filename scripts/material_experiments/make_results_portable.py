#!/usr/bin/env python3
"""Replace machine-local paths in result diagnostics with stable tokens."""

from __future__ import annotations

import csv
import json
import re
from pathlib import Path
from typing import Any


SCRIPT_DIR = Path(__file__).resolve().parent
REPOSITORY_ROOT = SCRIPT_DIR.parents[1].resolve()
RESULTS_DIR = SCRIPT_DIR / "results"
REPOSITORY_TOKEN = "${REPOSITORY_ROOT}"
RUN_TEMP_TOKEN = "${RUN_TEMP}"
TEMP_PATTERN = re.compile(
    r"(?i)[a-z]:[\\/]+users[\\/]+[^\\/]+[\\/]+appdata[\\/]+local"
    r"[\\/]+temp[\\/]+tvbperf_[^\\/\s\";]+"
)
ABSOLUTE_USER_PATH_PATTERN = re.compile(
    r"(?i)[a-z]:[\\/]+users[\\/]+"
)


def portable_text(value: str) -> str:
    result = value
    for root in {
        str(REPOSITORY_ROOT),
        REPOSITORY_ROOT.as_posix(),
    }:
        result = result.replace(root, REPOSITORY_TOKEN)
    return TEMP_PATTERN.sub(RUN_TEMP_TOKEN, result)


def portable_json(value: Any) -> Any:
    if isinstance(value, str):
        return portable_text(value)
    if isinstance(value, list):
        return [portable_json(item) for item in value]
    if isinstance(value, dict):
        return {key: portable_json(item) for key, item in value.items()}
    return value


def sanitize_json(path: Path) -> bool:
    had_bom = path.read_bytes().startswith(b"\xef\xbb\xbf")
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    portable = portable_json(value)
    if portable == value:
        return False
    encoding = "utf-8-sig" if had_bom else "utf-8"
    with path.open("w", encoding=encoding, newline="\n") as file:
        json.dump(portable, file, ensure_ascii=False, indent=2)
        file.write("\n")
    return True


def sanitize_csv(path: Path) -> bool:
    had_bom = path.read_bytes().startswith(b"\xef\xbb\xbf")
    with path.open("r", encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        fieldnames = list(reader.fieldnames or [])
        rows = list(reader)
    portable_rows = [
        {key: portable_text(value or "") for key, value in row.items()}
        for row in rows
    ]
    if portable_rows == rows:
        return False
    encoding = "utf-8-sig" if had_bom else "utf-8"
    with path.open("w", encoding=encoding, newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(portable_rows)
    return True


def remaining_local_path_files() -> list[str]:
    remaining: list[str] = []
    for path in sorted(RESULTS_DIR.rglob("*")):
        if path.suffix.lower() not in {".json", ".csv"} or not path.is_file():
            continue
        text = path.read_text(encoding="utf-8-sig", errors="replace")
        if ABSOLUTE_USER_PATH_PATTERN.search(text):
            remaining.append(path.relative_to(SCRIPT_DIR).as_posix())
    return remaining


def main() -> int:
    if RESULTS_DIR.resolve().parent != SCRIPT_DIR.resolve():
        raise RuntimeError(f"Unexpected results path: {RESULTS_DIR}")
    changed: list[str] = []
    for path in sorted(RESULTS_DIR.rglob("*")):
        if not path.is_file():
            continue
        did_change = False
        if path.suffix.lower() == ".json":
            did_change = sanitize_json(path)
        elif path.suffix.lower() == ".csv":
            did_change = sanitize_csv(path)
        if did_change:
            changed.append(path.relative_to(SCRIPT_DIR).as_posix())
    remaining = remaining_local_path_files()
    print(f"Portable result files updated: {len(changed)}")
    print(f"Files with remaining absolute user paths: {len(remaining)}")
    for path in remaining:
        print("  " + path)
    return 1 if remaining else 0


if __name__ == "__main__":
    raise SystemExit(main())
