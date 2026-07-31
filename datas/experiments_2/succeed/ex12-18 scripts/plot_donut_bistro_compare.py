#!/usr/bin/env python3
"""Plot the ex13 Donut renderer comparison for Bistro."""

from __future__ import annotations

import sys
from pathlib import Path

import plot_donut_sponza_compare as plot


def main() -> int:
    script_path = Path(__file__).resolve()
    script_dir = script_path.parent
    live_result_dir = script_dir / "results" / "ex13_donut_bistro_compare"
    archived_result_dir = (
        script_dir
        / "results"
        / "succeed"
        / "ex13_donut_bistro_compare"
    )
    result_dir = (
        live_result_dir if live_result_dir.exists() else archived_result_dir
    )

    plot.GPU_LABEL = "NVIDIA GeForce RTX 5060 Ti 16GB"
    plot.SCENE_LABEL = "Bistro"
    plot.ARTIFACT_STEM = "donut_bistro"
    plot.ARCHIVE_SCRIPT_PATH = script_path

    if len(sys.argv) == 1:
        sys.argv.extend(("--result-dir", str(result_dir)))
    return plot.main()


if __name__ == "__main__":
    raise SystemExit(main())
