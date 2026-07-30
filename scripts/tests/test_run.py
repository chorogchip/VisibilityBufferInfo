from __future__ import annotations

import csv
import json
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPTS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS_DIR))
import run  # noqa: E402


def valid_program_result_row() -> dict[str, str]:
    row = {field_name: "" for field_name in run.PROGRAM_RESULT_FIELDS}
    row.update(
        {
            "pass_name_0": "total",
            "pass_0_time_avg_ms": "1.25",
            "renderer_name": "VisBufMaterial",
            "run_current_time": "2026-07-29T20:00:00",
            "camera-mode-name": "free",
            "total_time_min_ms": "1.0",
            "total_time_median_ms": "1.2",
            "total_time_max_ms": "1.8",
            "total_time_avg_ms": "1.3",
            "total_time_p01_ms": "1.0",
            "total_time_p10_ms": "1.1",
            "total_time_p90_ms": "1.5",
            "total_time_p99_ms": "1.7",
            "source_material_count": "64",
            "active_material_bin_count": "8",
            "material_bin_compaction_ratio": "0.125",
            "variable-geometry-count": "1",
            "variable-overdraw-count": "0",
            "variable-waste-quad-count": "0",
            "variable-alu-op-count": "100",
        }
    )
    return row


class RunPolicyTests(unittest.TestCase):
    def test_closed_diagnostic_stream_does_not_abort_runner(self) -> None:
        class ClosedStream:
            def write(self, _: str) -> int:
                raise OSError(22, "Invalid argument")

            def flush(self) -> None:
                raise OSError(22, "Invalid argument")

        run.resilient_print("diagnostic", file=ClosedStream())

    def test_bom_json_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "spec.json"
            path.write_text(
                "\ufeff" + json.dumps({"base": {"variable": 1}}),
                encoding="utf-8",
            )
            self.assertEqual(run.read_json(path)["base"]["variable"], 1)

    def test_valid_program_result_csv_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "result.csv"
            with path.open("w", encoding="utf-8-sig", newline="") as file:
                writer = csv.DictWriter(file, fieldnames=run.PROGRAM_RESULT_FIELDS)
                writer.writeheader()
                writer.writerow(valid_program_result_row())
            rows = run.read_result_rows(path)
            self.assertEqual(len(rows), 1)

    def test_normal_stderr_does_not_salvage_a_success(self) -> None:
        rows = [valid_program_result_row()]
        status, error = run.classify_run(
            raw_rows=rows,
            return_code=0,
            process_error="",
            read_error="",
            stderr_text=(
                "Scene CPU cache disabled; rebuilding source scene.\n"
                "Log saved to: logs/example.log"
            ),
            failure_kind="",
            raw_path=Path("result.csv"),
        )
        self.assertEqual(status, "success")
        self.assertEqual(error, "")

    def test_nonzero_exit_with_valid_rows_is_salvaged(self) -> None:
        status, error = run.classify_run(
            raw_rows=[valid_program_result_row()],
            return_code=3,
            process_error="Process exited with code 3.",
            read_error="",
            stderr_text="",
            failure_kind="nonzero_exit",
            raw_path=Path("result.csv"),
        )
        self.assertEqual(status, "salvaged")
        self.assertIn("code 3", error)

    def test_timeout_is_failed_even_with_a_partial_row(self) -> None:
        status, _ = run.classify_run(
            raw_rows=[valid_program_result_row()],
            return_code=None,
            process_error="Process timed out after 1 second(s).",
            read_error="",
            stderr_text="",
            failure_kind="timeout",
            raw_path=Path("result.csv"),
        )
        self.assertEqual(status, "failed")

    def test_device_removed_diagnostic_is_failed(self) -> None:
        status, error = run.classify_run(
            raw_rows=[valid_program_result_row()],
            return_code=0,
            process_error="",
            read_error="",
            stderr_text="DXGI_ERROR_DEVICE_REMOVED",
            failure_kind="",
            raw_path=Path("result.csv"),
        )
        self.assertEqual(status, "failed")
        self.assertIn("untrustworthy", error)


if __name__ == "__main__":
    unittest.main()
