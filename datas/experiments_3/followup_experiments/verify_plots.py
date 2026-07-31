#!/usr/bin/env python3
"""Rebuild the follow-up plots twice and verify byte-for-byte reproducibility."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
PLOT_SCRIPT = ROOT / "plot_results.py"
PLOTS = ROOT / "plots"
REPORT = PLOTS / "data" / "reproducibility_report.json"


def hashes() -> dict[str, str]:
    return {
        path.relative_to(PLOTS).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(PLOTS.rglob("*"))
        if path.is_file() and path != REPORT
    }


def rebuild() -> None:
    subprocess.run([sys.executable, str(PLOT_SCRIPT)], check=True)


def main() -> int:
    rebuild()
    first = hashes()
    rebuild()
    second = hashes()

    missing = sorted(first.keys() - second.keys())
    added = sorted(second.keys() - first.keys())
    changed = sorted(
        path for path in first.keys() & second.keys() if first[path] != second[path]
    )
    status = "reproducible" if not (missing or added or changed) else "mismatch"
    report = {
        "status": status,
        "generation_count": 2,
        "compared_file_count": len(first),
        "hash_algorithm": "sha256",
        "missing_after_second_run": missing,
        "added_after_second_run": added,
        "changed_after_second_run": changed,
        "generator": PLOT_SCRIPT.name,
    }
    REPORT.write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(report, indent=2))
    return 0 if status == "reproducible" else 1


if __name__ == "__main__":
    raise SystemExit(main())
