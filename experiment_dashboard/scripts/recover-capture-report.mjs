import { readFile, readdir, writeFile } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { parse } from "csv-parse/sync";

const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const resultRoot = join(
  projectRoot,
  "capture_specs",
  "results",
  "dashboard_camera_capture",
);
const runRoot = join(resultRoot, "dashboard_camera_capture_runs");
const reportPath = join(resultRoot, "dashboard_camera_capture_run_report.json");
const csvPath = join(resultRoot, "dashboard_camera_capture.csv");
const dashboardPath = join(projectRoot, "public", "data", "dashboard.json");

const rows = parse(await readFile(csvPath, "utf8"), {
  bom: true,
  columns: true,
  skip_empty_lines: true,
});
if (rows.length !== 7) {
  throw new Error(`Expected 7 preserved result rows, found ${rows.length}.`);
}

const runIndexes = rows
  .map((row) => Number(row.runner_run_index))
  .sort((a, b) => a - b);
if (
  runIndexes.some((value, index) => value !== index) ||
  rows.some(
    (row) =>
      row.runner_status !== "success" ||
      Number(row.runner_return_code) !== 0 ||
      row.runner_error !== "",
  )
) {
  throw new Error("Preserved result rows are not a clean 7/7 success set.");
}

const runs = [];
for (const row of rows) {
  const runIndex = Number(row.runner_run_index);
  const runName = `run_${String(runIndex).padStart(5, "0")}_capture`;
  const manifestPath = join(runRoot, runName, "capture_manifest.json");
  const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
  const frameFiles = (await readdir(join(runRoot, runName, "frames"))).filter(
    (name) => name.toLowerCase().endsWith(".png"),
  );
  if (
    manifest.captured_frame_count !== frameFiles.length ||
    manifest.frames.length !== frameFiles.length
  ) {
    throw new Error(`${runName}: manifest and PNG counts do not match.`);
  }
  runs.push({
    run_index: runIndex,
    status: row.runner_status,
    return_code: Number(row.runner_return_code),
    error: row.runner_error,
    diagnostic_stderr: row.runner_stderr,
    result_recorded_at: row.run_current_time,
    capture_manifest: `dashboard_camera_capture_runs/${runName}/capture_manifest.json`,
    captured_frames: frameFiles.length,
  });
}

const priorDashboard = JSON.parse(await readFile(dashboardPath, "utf8"));
const report = {
  status: "completed",
  started_at: null,
  finished_at: priorDashboard.provenance.captureFinishedAt,
  last_updated_at: new Date().toISOString(),
  experiment: "dashboard_camera_capture",
  expected_runs: 7,
  total_runs: 7,
  completed_runs: 7,
  successful_runs: 7,
  salvaged_runs: 0,
  failed_runs: 0,
  skipped_runs: 0,
  spec_path: "capture_specs/dashboard_camera_capture.json",
  output_csv:
    "capture_specs/results/dashboard_camera_capture/dashboard_camera_capture.csv",
  report_json:
    "capture_specs/results/dashboard_camera_capture/dashboard_camera_capture_run_report.json",
  runs,
  recovered_from_preserved_evidence: true,
  recovery_note:
    "The mutable top-level report was unexpectedly reinitialized after the completed capture. This summary was rebuilt from seven preserved success CSV rows, seven capture manifests, 348 PNG files, and the previously generated dashboard completion timestamp. Measurement rows and capture bytes were not regenerated.",
  recovered_evidence: {
    csv_rows: rows.length,
    manifest_count: runs.length,
    png_count: runs.reduce((sum, run) => sum + run.captured_frames, 0),
    prior_completion_timestamp:
      priorDashboard.provenance.captureFinishedAt,
  },
};

await writeFile(reportPath, `${JSON.stringify(report, null, 2)}\n`);
console.log(
  `Recovered completed capture report from ${rows.length} rows, ` +
    `${runs.length} manifests, and ${report.recovered_evidence.png_count} PNG files.`,
);
