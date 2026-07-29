import { createHash } from "node:crypto";
import {
  access,
  mkdir,
  readFile,
  readdir,
  rm,
  stat,
  writeFile,
} from "node:fs/promises";
import { dirname, extname, isAbsolute, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { parse } from "csv-parse/sync";
import sharp from "sharp";

const PROJECT_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const REPOSITORY_ROOT = resolve(PROJECT_ROOT, "..");
const CAMPAIGN_ROOT = join(
  REPOSITORY_ROOT,
  "scripts",
  "material_experiments",
);
const CAMPAIGN_DATA = join(CAMPAIGN_ROOT, "plots", "data");
const CAPTURE_ROOT = join(
  PROJECT_ROOT,
  "capture_specs",
  "results",
  "dashboard_camera_capture",
);
const CAPTURE_RUNS = join(
  CAPTURE_ROOT,
  "dashboard_camera_capture_runs",
);
const PUBLIC_DATA = join(PROJECT_ROOT, "public", "data");
const PUBLIC_CAPTURES = join(PUBLIC_DATA, "captures");
const PUBLIC_VALIDATION = join(PUBLIC_DATA, "validation");
const DASHBOARD_JSON = join(PUBLIC_DATA, "dashboard.json");
const CURRENT_DATASET_ID = "manual-20260729-rtx5060ti16";
const HISTORICAL_DATASET_ID = "historical-rtx5070";
const BARYCENTRIC_ROOT = join(
  REPOSITORY_ROOT,
  "scripts",
  "barycentric_validation",
);
const BARYCENTRIC_FRAME_METRICS = join(
  BARYCENTRIC_ROOT,
  "analysis",
  "data",
  "frame_metrics.csv",
);
const BARYCENTRIC_HEATMAPS = join(
  BARYCENTRIC_ROOT,
  "analysis",
  "local_heatmaps",
);
const SOFTWARE_RASTER_FRAMES = join(
  REPOSITORY_ROOT,
  "scripts",
  "followup_experiments",
  "plots",
  "data",
  "18_software_raster_frames.csv",
);

const HISTORICAL_CAMERA_BASELINES = [
  {
    scene: "Sponza",
    path: join(
      REPOSITORY_ROOT,
      "datas",
      "experiments",
      "succeed",
      "ex12_donut_sponza_compare",
      "plots",
      "donut_sponza_summary.csv",
    ),
  },
  {
    scene: "Bistro",
    path: join(
      REPOSITORY_ROOT,
      "datas",
      "experiments",
      "succeed",
      "ex13_donut_bistro_compare",
      "plots",
      "donut_bistro_summary.csv",
    ),
  },
  {
    scene: "SponzaIvy",
    path: join(
      REPOSITORY_ROOT,
      "datas",
      "experiments",
      "succeed",
      "ex14_donut_sponza_ivy_compare",
      "plots",
      "donut_sponza_ivy_summary.csv",
    ),
  },
];

const EXPERIMENT_META = {
  "00_smoke_synthetic_material_grid.json": {
    title: "Synthetic material smoke",
    group: "Synthetic",
    xKey: "variable",
    xLabel: "Reference case",
  },
  "01_smoke_real_pbr_feature.json": {
    title: "Real-scene PBR smoke",
    group: "Real scene",
    xKey: "renderer_variant",
    xLabel: "Renderer",
  },
  "10_synth_locality_fixed64.json": {
    title: "Synthetic locality",
    group: "Synthetic",
    xKey: "material_assign_locality",
    xLabel: "Locality",
  },
  "11_synth_diversity_fixed64.json": {
    title: "Synthetic diversity",
    group: "Synthetic",
    xKey: "material_assign_diversity",
    xLabel: "Diversity",
  },
  "12_synth_locality_diversity_map_fixed64.json": {
    title: "Locality × diversity map",
    group: "Synthetic",
    xKey: "material_assign_locality",
    xLabel: "Locality",
    secondaryKey: "material_assign_diversity",
    secondaryLabel: "Diversity",
  },
  "13_synth_open_bin_count_diagnostic.json": {
    title: "Open-bin class count",
    group: "Synthetic",
    xKey: "material_assign_max_open",
    xLabel: "Maximum open classes",
  },
  "14_synth_workload_scaling.json": {
    title: "Synthetic workload scaling",
    group: "Scaling",
    xKey: "geometry_div",
    xLabel: "Geometry subdivision",
  },
  "15_synth_seed_robustness.json": {
    title: "Seed robustness",
    group: "Robustness",
    xKey: "seed",
    xLabel: "Seed",
  },
  "16_synth_renderer_compare_selected.json": {
    title: "Synthetic renderer comparison",
    group: "Renderer",
    xKey: "renderer_variant",
    xLabel: "Renderer",
  },
  "20_real_random_bin_count_quick_all_scenes.json": {
    title: "Real-scene class count",
    group: "Real scene",
    xKey: "material_assign_max_open",
    xLabel: "Maximum open classes",
  },
  "21_real_random_diversity_quick_all_scenes.json": {
    title: "Real-scene diversity",
    group: "Real scene",
    xKey: "material_assign_diversity",
    xLabel: "Diversity",
  },
  "22_real_pbr_feature_renderer_compare_full.json": {
    title: "PBR renderer comparison",
    group: "Renderer",
    xKey: "renderer_variant",
    xLabel: "Renderer",
  },
  "23_real_random_selected_renderer_compare_quick.json": {
    title: "Random-material renderer comparison",
    group: "Renderer",
    xKey: "renderer_variant",
    xLabel: "Renderer",
  },
  "24_real_resolution_scaling_pbr_quick.json": {
    title: "Resolution scaling",
    group: "Scaling",
    xKey: "window_width",
    xLabel: "Width (px)",
  },
  "25_real_texture_vfc_ablation_quick.json": {
    title: "Texture / VFC ablation",
    group: "Ablation",
    xKey: "use_vfc",
    xLabel: "View-frustum culling",
    secondaryKey: "to_load_texture",
    secondaryLabel: "Textures",
  },
  "30_final_selected_full_camera.json": {
    title: "Full camera-path comparison",
    group: "Camera",
    xKey: "variable",
    xLabel: "Camera scenario",
  },
  "31_capture_representative_frames.json": {
    title: "Representative frame capture",
    group: "Camera",
    xKey: "variable",
    xLabel: "Capture case",
  },
  "32_nsight_representative_cases.json": {
    title: "Representative profiling cases",
    group: "Profiling",
    xKey: "variable",
    xLabel: "Profiling case",
  },
};

const SEQUENCES = [
  {
    runIndex: 0,
    id: "sponza-pbr",
    label: "Sponza · PBR",
    scene: "Sponza",
    viewKind: "beauty",
    view: "pbr",
    viewLabel: "PBR shaded",
    renderer: "DonutVisGBuffer",
    rendererVariant: 9,
    debugMode: null,
    description: "Textured PBR output along the complete Sponza camera path.",
    expectedFrames: 42,
    profileId: "sponza-visbuf",
  },
  {
    runIndex: 1,
    id: "bistro-pbr",
    label: "Bistro · PBR",
    scene: "Bistro",
    viewKind: "beauty",
    view: "pbr",
    viewLabel: "PBR shaded",
    renderer: "DonutVisGBuffer",
    rendererVariant: 9,
    debugMode: null,
    description: "Textured PBR output along the complete Bistro camera path.",
    expectedFrames: 92,
    profileId: "bistro-visbuf",
  },
  {
    runIndex: 2,
    id: "sponza-geometry-instance",
    label: "Sponza · Geometry instances",
    scene: "Sponza",
    viewKind: "debug",
    view: "geometry-instance",
    viewLabel: "Geometry instance ID",
    renderer: "DonutVisDebug",
    rendererVariant: 13,
    debugMode: 0,
    description: "Stable colors identify geometry-instance boundaries.",
    expectedFrames: 42,
    profileId: "sponza-visbuf",
  },
  {
    runIndex: 3,
    id: "sponza-primitive",
    label: "Sponza · Primitives",
    scene: "Sponza",
    viewKind: "debug",
    view: "primitive",
    viewLabel: "Primitive ID",
    renderer: "DonutVisDebug",
    rendererVariant: 13,
    debugMode: 1,
    description: "A hashed color per primitive exposes triangle density.",
    expectedFrames: 42,
    profileId: "sponza-visbuf",
  },
  {
    runIndex: 4,
    id: "sponza-geometry-primitive",
    label: "Sponza · Combined IDs",
    scene: "Sponza",
    viewKind: "debug",
    view: "geometry-primitive",
    viewLabel: "Geometry + primitive ID",
    renderer: "DonutVisDebug",
    rendererVariant: 13,
    debugMode: 2,
    description: "Geometry and primitive identities are hashed together.",
    expectedFrames: 42,
    profileId: "sponza-visbuf",
  },
  {
    runIndex: 5,
    id: "sponza-barycentric",
    label: "Sponza · Barycentrics",
    scene: "Sponza",
    viewKind: "debug",
    view: "barycentric",
    viewLabel: "Barycentric coordinates",
    renderer: "DonutVisDebug",
    rendererVariant: 13,
    debugMode: 3,
    description: "RGB encodes the reconstructed barycentric coordinates.",
    expectedFrames: 42,
    profileId: "sponza-visbuf",
  },
  {
    runIndex: 6,
    id: "bistro-geometry-primitive",
    label: "Bistro · Combined IDs",
    scene: "Bistro",
    viewKind: "debug",
    view: "geometry-primitive",
    viewLabel: "Geometry + primitive ID",
    renderer: "DonutVisDebug",
    rendererVariant: 13,
    debugMode: 2,
    description: "Combined visibility identity across the longer Bistro path.",
    expectedFrames: 46,
    profileId: "bistro-visbuf",
  },
];

const SUPPLEMENTAL_SEQUENCES = [
  {
    id: "sponza-ivy-pbr",
    label: "Sponza + Ivy · PBR",
    scene: "SponzaIvy",
    viewKind: "beauty",
    view: "pbr",
    viewLabel: "PBR shaded",
    renderer: "DonutVisGBuffer",
    rendererVariant: 9,
    debugMode: null,
    description:
      "Textured PBR anchor frames from the Sponza + Ivy camera path.",
    expectedFrames: 5,
    profileId: "sponza-ivy-visbuf",
    sourceDir: join(
      CAMPAIGN_ROOT,
      "results",
      "31_capture_representative_frames",
      "31_capture_representative_frames_runs",
      "run_00007_capture",
    ),
    sourceRunIndex: 7,
  },
];

const VALIDATION_MODES = {
  linear_barycentric: {
    order: 0,
    label: "Linear barycentrics",
    shortLabel: "Linear bary",
    description:
      "No-perspective barycentric coordinates reconstructed from the visibility buffer.",
  },
  perspective_barycentric: {
    order: 1,
    label: "Perspective barycentrics",
    shortLabel: "Perspective bary",
    description:
      "Perspective-correct barycentric coordinates used by attribute reconstruction.",
  },
  linear_barycentric_dx: {
    order: 2,
    label: "Barycentric ddx",
    shortLabel: "Bary ddx",
    description:
      "Screen-space x derivative of reconstructed linear barycentrics.",
  },
  linear_barycentric_dy: {
    order: 3,
    label: "Barycentric ddy",
    shortLabel: "Bary ddy",
    description:
      "Screen-space y derivative of reconstructed linear barycentrics.",
  },
  uv_dx: {
    order: 4,
    label: "UV ddx",
    shortLabel: "UV ddx",
    description:
      "Screen-space x derivative of the reconstructed texture coordinates.",
  },
  uv_dy: {
    order: 5,
    label: "UV ddy",
    shortLabel: "UV ddy",
    description:
      "Screen-space y derivative of the reconstructed texture coordinates.",
  },
  uv_lod_proxy_1024px: {
    order: 6,
    label: "Texture LOD proxy",
    shortLabel: "LOD proxy",
    description:
      "A 1024 px texture-footprint proxy derived from the reconstructed UV gradients.",
  },
};

const PROFILE_SOURCES = [
  {
    id: "sponza-deferred",
    scene: "Sponza",
    renderer: "DonutDeferredPrepass",
    runIndex: 0,
  },
  {
    id: "sponza-visbuf",
    scene: "Sponza",
    renderer: "DonutVisGBuffer",
    runIndex: 1,
  },
  {
    id: "bistro-deferred",
    scene: "Bistro",
    renderer: "DonutDeferredPrepass",
    runIndex: 4,
  },
  {
    id: "bistro-visbuf",
    scene: "Bistro",
    renderer: "DonutVisGBuffer",
    runIndex: 5,
  },
  {
    id: "sponza-ivy-deferred",
    scene: "SponzaIvy",
    renderer: "DonutDeferredPrepass",
    runIndex: 8,
  },
  {
    id: "sponza-ivy-visbuf",
    scene: "SponzaIvy",
    renderer: "DonutVisGBuffer",
    runIndex: 9,
  },
];

const HARDWARE = {
  id: CURRENT_DATASET_ID,
  gpu: {
    name: "NVIDIA GeForce RTX 5060 Ti 16GB",
    driver: "610.62",
    memoryMiB: 16311,
    powerLimitW: 180,
    maxGraphicsClockMHz: 3090,
    maxMemoryClockMHz: 14001,
    computeCapability: "12.0",
  },
  cpu: {
    name: "Intel Core Ultra 7 265KF",
    cores: 20,
    logicalProcessors: 20,
    maxClockMHz: 3900,
  },
  system: {
    memoryBytes: 33974763520,
    os: "Windows 11 Home",
    build: "26200",
  },
  toolchain: {
    visualStudio: "Visual Studio 2026 18.8.1",
    msvc: "14.51.36231",
    python: "3.14.0",
    build: "x64-Release · Ninja · single-config",
  },
};

const DATASETS = [
  {
    id: CURRENT_DATASET_ID,
    label: "Today · RTX 5060 Ti 16GB",
    gpu: "NVIDIA GeForce RTX 5060 Ti 16GB",
    role: "current",
    measuredOn: "2026-07-29",
    scope: "396-run material campaign, camera profiles, and all frame captures",
    includedInExperimentExplorer: true,
  },
  {
    id: HISTORICAL_DATASET_ID,
    label: "Previous · RTX 5070",
    gpu: "NVIDIA GeForce RTX 5070",
    role: "historical",
    measuredOn: null,
    scope: "Archived Sponza, Bistro, and Sponza + Ivy camera baselines",
    includedInExperimentExplorer: false,
    attributionNote:
      "Owner-provided provenance identifies these archived results as RTX 5070 measurements; inherited CSV GPU labels are not used.",
  },
];

function asNumber(value) {
  if (value === null || value === undefined || value === "") return null;
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : null;
}

function asParameter(value) {
  if (value === "") return null;
  const numeric = asNumber(value);
  return numeric === null ? value : numeric;
}

function json(value) {
  return `${JSON.stringify(value, null, 2)}\n`;
}

async function readJson(path) {
  return JSON.parse(await readFile(path, "utf8"));
}

async function readCsv(path) {
  return parse(await readFile(path, "utf8"), {
    bom: true,
    columns: true,
    skip_empty_lines: true,
  });
}

async function sha256(path) {
  const digest = createHash("sha256");
  digest.update(await readFile(path));
  return digest.digest("hex");
}

async function exists(path) {
  try {
    await access(path);
    return true;
  } catch {
    return false;
  }
}

async function writeFileWithRetry(path, contents) {
  let lastError;
  for (let attempt = 0; attempt < 5; attempt += 1) {
    try {
      await writeFile(path, contents);
      return;
    } catch (error) {
      lastError = error;
      if (!["EPERM", "EBUSY", "UNKNOWN"].includes(error?.code)) throw error;
      await new Promise((resolvePromise) =>
        setTimeout(resolvePromise, 100 * (attempt + 1)),
      );
    }
  }
  throw lastError;
}

function resultRows(records) {
  return records.map((record) => {
    const parameters = Object.fromEntries(
      Object.entries(record)
        .filter(([key]) => key.startsWith("param_"))
        .map(([key, value]) => [key.slice(6), asParameter(value)]),
    );
    const passes = [];
    for (let index = 0; index < 32; index += 1) {
      const name = record[`pass_name_${index}`];
      const timeAvgMs = asNumber(record[`pass_${index}_time_avg_ms`]);
      if (name && timeAvgMs !== null) {
        passes.push({ name, timeAvgMs });
      }
    }
    return {
      id: `${record.config}:${record.runner_run_index}:${record.runner_repeat}`,
      hardwareId: CURRENT_DATASET_ID,
      config: record.config,
      scene: record.scene,
      renderer: record.renderer_name,
      status: record.runner_status,
      runIndex: asNumber(record.runner_run_index),
      repeat: asNumber(record.runner_repeat),
      metrics: {
        minMs: asNumber(record.total_time_min_ms),
        medianMs: asNumber(record.total_time_median_ms),
        maxMs: asNumber(record.total_time_max_ms),
        avgMs: asNumber(record.total_time_avg_ms),
        p90Ms: asNumber(record.total_time_p90_ms),
        p99Ms: asNumber(record.total_time_p99_ms),
      },
      parameters,
      passes,
    };
  });
}

function experimentCatalog(rows) {
  const counts = new Map();
  for (const row of rows) counts.set(row.config, (counts.get(row.config) ?? 0) + 1);
  return Object.entries(EXPERIMENT_META).map(([config, metadata]) => ({
    config,
    ...metadata,
    rowCount: counts.get(config) ?? 0,
  }));
}

async function cameraProfiles() {
  const runRoot = join(
    CAMPAIGN_ROOT,
    "results",
    "30_final_selected_full_camera",
    "30_final_selected_full_camera_runs",
  );
  const profiles = [];
  for (const profile of PROFILE_SOURCES) {
    const path = join(
      runRoot,
      `run_${String(profile.runIndex).padStart(5, "0")}.csv_${profile.runIndex}_result.csv`,
    );
    const records = await readCsv(path);
    const samples = records.map((record) => ({
      frame: asNumber(record.frame),
      totalMs: asNumber(record.total),
      indexCount: asNumber(record.index_count),
      passes: Object.fromEntries(
        Object.entries(record)
          .filter(([key]) => !["frame", "total", "index_count"].includes(key))
          .map(([key, value]) => [key, asNumber(value)]),
      ),
    }));
    profiles.push({ ...profile, hardwareId: CURRENT_DATASET_ID, samples });
  }
  return profiles;
}

async function hardwareComparison(rows) {
  const current = rows
    .filter(
      (row) =>
        row.config === "30_final_selected_full_camera.json" &&
        row.parameters.material_assign_strategy === 1 &&
        [8, 9].includes(row.parameters.renderer_variant),
    )
    .map((row) => ({
      id: `${CURRENT_DATASET_ID}:${row.scene}:${row.renderer}`,
      datasetId: CURRENT_DATASET_ID,
      hardwareLabel: "RTX 5060 Ti 16GB",
      scene: row.scene,
      renderer: row.renderer,
      rendererVariant: row.parameters.renderer_variant,
      resolution: `${row.parameters.window_width}x${row.parameters.window_height}`,
      warmupFrames: row.parameters.warmup_frames,
      measureFrames: row.parameters.measure_frames,
      profileWindowFrames: row.parameters.profile_window_frames,
      vfc: row.parameters.use_vfc,
      textures: row.parameters.to_load_texture,
      metrics: row.metrics,
      source: "30_final_selected_full_camera.json",
    }));

  const historical = [];
  for (const baseline of HISTORICAL_CAMERA_BASELINES) {
    const records = await readCsv(baseline.path);
    for (const record of records) {
      const rendererVariant = asNumber(record.renderer_variant);
      if (![8, 9].includes(rendererVariant)) continue;
      historical.push({
        id: `${HISTORICAL_DATASET_ID}:${baseline.scene}:${record.renderer_name}`,
        datasetId: HISTORICAL_DATASET_ID,
        hardwareLabel: "RTX 5070",
        scene: baseline.scene,
        renderer: record.renderer_name,
        rendererVariant,
        resolution: record.resolution,
        warmupFrames: asNumber(record.warmup_frames),
        measureFrames: asNumber(record.measure_frames),
        profileWindowFrames: asNumber(record.profile_window_frames),
        vfc: asNumber(record.vfc),
        textures: asNumber(record.textures),
        metrics: {
          avgMs: asNumber(record.total_avg_ms),
          medianMs: asNumber(record.total_median_ms),
          p90Ms: asNumber(record.total_p90_ms),
          p99Ms: asNumber(record.total_p99_ms),
        },
        source: `datas/experiments/succeed/${baseline.path.split("succeed")[1].replaceAll("\\", "/").replace(/^\//, "")}`,
      });
    }
  }

  if (current.length !== 6 || historical.length !== 6) {
    throw new Error(
      `Expected 6 current and 6 historical matched camera baselines; found ${current.length} and ${historical.length}.`,
    );
  }

  const condition = (row) =>
    JSON.stringify([
      row.scene,
      row.rendererVariant,
      row.resolution,
      row.warmupFrames,
      row.measureFrames,
      row.vfc,
      row.textures,
    ]);
  const historicalConditions = new Set(historical.map(condition));
  for (const row of current) {
    if (!historicalConditions.has(condition(row))) {
      throw new Error(`No matched RTX 5070 baseline for ${row.scene}/${row.renderer}.`);
    }
  }
  return [...current, ...historical];
}

async function mapWithConcurrency(items, concurrency, callback) {
  let cursor = 0;
  const workers = Array.from(
    { length: Math.min(concurrency, items.length) },
    async () => {
      while (cursor < items.length) {
        const index = cursor;
        cursor += 1;
        await callback(items[index], index);
      }
    },
  );
  await Promise.all(workers);
}

async function captureSequences({ write }) {
  const captureReport = await readJson(
    join(CAPTURE_ROOT, "dashboard_camera_capture_run_report.json"),
  );
  if (
    captureReport.status !== "completed" ||
    captureReport.successful_runs !== SEQUENCES.length ||
    captureReport.salvaged_runs !== 0 ||
    captureReport.failed_runs !== 0 ||
    captureReport.skipped_runs !== 0
  ) {
    throw new Error("Capture run report is not a clean 7/7 completion.");
  }

  if (write) {
    await rm(PUBLIC_CAPTURES, { recursive: true, force: true });
    await mkdir(PUBLIC_CAPTURES, { recursive: true });
  }

  const sequences = [];
  for (const sequence of SEQUENCES) {
    const runName = `run_${String(sequence.runIndex).padStart(5, "0")}_capture`;
    const sourceDir = join(CAPTURE_RUNS, runName);
    const sourceFrames = join(sourceDir, "frames");
    const sourceManifest = await readJson(join(sourceDir, "capture_manifest.json"));
    const sourceFiles = (await readdir(sourceFrames))
      .filter((name) => extname(name).toLowerCase() === ".png")
      .sort();
    if (
      sourceManifest.captured_frame_count !== sequence.expectedFrames ||
      sourceFiles.length !== sequence.expectedFrames ||
      sourceManifest.frames.length !== sequence.expectedFrames
    ) {
      throw new Error(
        `${sequence.id}: expected ${sequence.expectedFrames} frames, found ${sourceFiles.length}.`,
      );
    }

    const targetDir = join(PUBLIC_CAPTURES, sequence.id);
    if (write) await mkdir(targetDir, { recursive: true });
    const frameRecords = [];
    const excludedBlankFrames = [];

    await mapWithConcurrency(sourceManifest.frames, 4, async (sourceFrame) => {
      const sourceName = `frame_${String(sourceFrame.image_index).padStart(6, "0")}.png`;
      if (!sourceFiles.includes(sourceName)) {
        throw new Error(`${sequence.id}: missing ${sourceName}.`);
      }
      const targetName = `${String(sourceFrame.image_index).padStart(4, "0")}.webp`;
      const sourcePath = join(sourceFrames, sourceName);
      const targetPath = join(targetDir, targetName);
      const sourceStats = await sharp(sourcePath).stats();
      const sourceMeanRgb =
        sourceStats.channels.slice(0, 3).reduce((sum, channel) => sum + channel.mean, 0) /
        3;
      const sourceVariationRgb =
        sourceStats.channels
          .slice(0, 3)
          .reduce((sum, channel) => sum + channel.stdev, 0) / 3;
      if (sourceMeanRgb < 1 || sourceVariationRgb < 0.25) {
        excludedBlankFrames.push(sourceFrame.image_index);
        return;
      }
      if (write) {
        const rgb = await sharp(sourcePath, { failOn: "error" })
          .removeAlpha()
          .raw()
          .toBuffer({ resolveWithObject: true });
        const pipeline = sharp(rgb.data, {
          raw: {
            width: rgb.info.width,
            height: rgb.info.height,
            channels: 3,
          },
        }).resize(960, 540, { fit: "fill" });
        if (sequence.viewKind === "debug") {
          await pipeline
            .webp({ quality: 90, effort: 6, smartSubsample: true })
            .toFile(targetPath);
        } else {
          await pipeline
            .webp({ quality: 84, effort: 6, smartSubsample: true })
            .toFile(targetPath);
        }
      }
      if (!(await exists(targetPath))) {
        throw new Error(`${sequence.id}: missing deployable ${targetName}.`);
      }
      const metadata = await sharp(targetPath).metadata();
      if (metadata.width !== 960 || metadata.height !== 540) {
        throw new Error(
          `${sequence.id}/${targetName}: expected 960x540, found ${metadata.width}x${metadata.height}.`,
        );
      }
      if (metadata.hasAlpha) {
        throw new Error(`${sequence.id}/${targetName}: unexpected alpha channel.`);
      }
      const imageStats = await sharp(targetPath).stats();
      const meanRgb =
        imageStats.channels.slice(0, 3).reduce((sum, channel) => sum + channel.mean, 0) /
        3;
      if (meanRgb < 1) {
        throw new Error(
          `${sequence.id}/${targetName}: deployable frame is effectively black.`,
        );
      }
      const fileStat = await stat(targetPath);
      frameRecords[sourceFrame.image_index] = {
        imageIndex: sourceFrame.image_index,
        renderFrame: sourceFrame.render_frame,
        measurementFrame: sourceFrame.measurement_frame,
        src: `/data/captures/${sequence.id}/${targetName}`,
        bytes: fileStat.size,
        sha256: await sha256(targetPath),
      };
    });

    const deployableFrames = frameRecords.filter(Boolean);
    const publicManifest = {
      id: sequence.id,
      label: sequence.label,
      scene: sequence.scene,
      view: sequence.view,
      viewLabel: sequence.viewLabel,
      viewKind: sequence.viewKind,
      renderer: sequence.renderer,
      rendererVariant: sequence.rendererVariant,
      debugMode: sequence.debugMode,
      description: sequence.description,
      profileId: sequence.profileId,
      source: {
        hardwareId: CURRENT_DATASET_ID,
        runIndex: sequence.runIndex,
        width: sourceManifest.width,
        height: sourceManifest.height,
        warmupFrames: sourceManifest.warmup_frames,
        measureFrames: sourceManifest.measure_frames,
        captureStride: sourceManifest.capture_stride,
        captureFps: sourceManifest.capture_fps,
        capturedFrames: sourceManifest.captured_frame_count,
      },
      excludedBlankFrames: excludedBlankFrames.sort((a, b) => a - b),
      frameCount: deployableFrames.length,
      frames: deployableFrames,
    };
    if (write) {
      await writeFile(join(targetDir, "manifest.json"), json(publicManifest));
    }
    sequences.push(publicManifest);
  }
  return { sequences, captureReport };
}

async function supplementalCaptureSequences({ write }) {
  const sequences = [];
  for (const sequence of SUPPLEMENTAL_SEQUENCES) {
    const sourceFrames = join(sequence.sourceDir, "frames");
    const sourceManifest = await readJson(
      join(sequence.sourceDir, "capture_manifest.json"),
    );
    const sourceFiles = (await readdir(sourceFrames))
      .filter((name) => extname(name).toLowerCase() === ".png")
      .sort();
    if (
      sourceManifest.captured_frame_count !== sequence.expectedFrames ||
      sourceFiles.length !== sequence.expectedFrames ||
      sourceManifest.frames.length !== sequence.expectedFrames
    ) {
      throw new Error(
        `${sequence.id}: expected ${sequence.expectedFrames} supplemental frames, found ${sourceFiles.length}.`,
      );
    }

    const targetDir = join(PUBLIC_CAPTURES, sequence.id);
    if (write) await mkdir(targetDir, { recursive: true });
    const frameRecords = [];
    await mapWithConcurrency(sourceManifest.frames, 4, async (sourceFrame) => {
      const sourceName = `frame_${String(sourceFrame.image_index).padStart(6, "0")}.png`;
      const targetName = `${String(sourceFrame.image_index).padStart(4, "0")}.webp`;
      const sourcePath = join(sourceFrames, sourceName);
      const targetPath = join(targetDir, targetName);
      if (!sourceFiles.includes(sourceName)) {
        throw new Error(`${sequence.id}: missing ${sourceName}.`);
      }
      if (write) {
        const rgb = await sharp(sourcePath, { failOn: "error" })
          .removeAlpha()
          .raw()
          .toBuffer({ resolveWithObject: true });
        await sharp(rgb.data, {
          raw: {
            width: rgb.info.width,
            height: rgb.info.height,
            channels: 3,
          },
        })
          .resize(960, 540, { fit: "fill" })
          .webp({ quality: 84, effort: 6, smartSubsample: true })
          .toFile(targetPath);
      }
      const metadata = await sharp(targetPath).metadata();
      if (metadata.width !== 960 || metadata.height !== 540) {
        throw new Error(
          `${sequence.id}/${targetName}: expected 960x540, found ${metadata.width}x${metadata.height}.`,
        );
      }
      const imageStats = await sharp(targetPath).stats();
      const meanRgb =
        imageStats.channels
          .slice(0, 3)
          .reduce((sum, channel) => sum + channel.mean, 0) / 3;
      const variationRgb =
        imageStats.channels
          .slice(0, 3)
          .reduce((sum, channel) => sum + channel.stdev, 0) / 3;
      if (meanRgb < 1 || variationRgb < 0.25) {
        throw new Error(
          `${sequence.id}/${targetName}: deployable frame is effectively blank.`,
        );
      }
      const fileStat = await stat(targetPath);
      frameRecords[sourceFrame.image_index] = {
        imageIndex: sourceFrame.image_index,
        renderFrame: sourceFrame.render_frame,
        measurementFrame: sourceFrame.measurement_frame,
        src: `/data/captures/${sequence.id}/${targetName}`,
        bytes: fileStat.size,
        sha256: await sha256(targetPath),
      };
    });

    const publicManifest = {
      id: sequence.id,
      label: sequence.label,
      scene: sequence.scene,
      view: sequence.view,
      viewLabel: sequence.viewLabel,
      viewKind: sequence.viewKind,
      renderer: sequence.renderer,
      rendererVariant: sequence.rendererVariant,
      debugMode: sequence.debugMode,
      description: sequence.description,
      profileId: sequence.profileId,
      source: {
        hardwareId: CURRENT_DATASET_ID,
        runIndex: sequence.sourceRunIndex,
        width: sourceManifest.width,
        height: sourceManifest.height,
        warmupFrames: sourceManifest.warmup_frames,
        measureFrames: sourceManifest.measure_frames,
        captureStride: sourceManifest.capture_stride,
        captureFps: sourceManifest.capture_fps,
        capturedFrames: sourceManifest.captured_frame_count,
        sourceExperiment: "31_capture_representative_frames.json",
      },
      excludedBlankFrames: [],
      frameCount: frameRecords.length,
      frames: frameRecords,
    };
    if (write) {
      await writeFile(join(targetDir, "manifest.json"), json(publicManifest));
    }
    sequences.push(publicManifest);
  }
  return sequences;
}

function validationSceneKey(scene) {
  if (scene === "Sponza Ivy") return "SponzaIvy";
  return scene;
}

function validationSceneFolder(scene) {
  if (scene === "Sponza Ivy") return "sponza_ivy";
  return scene.toLowerCase();
}

async function barycentricValidationSequences({ write }) {
  const records = await readCsv(BARYCENTRIC_FRAME_METRICS);
  if (records.length !== 910) {
    throw new Error(
      `Expected 910 barycentric validation pairs, found ${records.length}.`,
    );
  }
  if (write) {
    await rm(PUBLIC_VALIDATION, { recursive: true, force: true });
    await mkdir(PUBLIC_VALIDATION, { recursive: true });
  }

  const grouped = new Map();
  for (const record of records) {
    const metadata = VALIDATION_MODES[record.mode_name];
    if (!metadata) {
      throw new Error(`Unknown validation mode: ${record.mode_name}.`);
    }
    const scene = validationSceneKey(record.scene);
    const id = `${scene.toLowerCase()}-${record.mode_name.replaceAll("_", "-")}`;
    const group = grouped.get(id) ?? {
      id,
      scene,
      mode: Number(record.mode),
      modeName: record.mode_name,
      label: metadata.label,
      shortLabel: metadata.shortLabel,
      order: metadata.order,
      description: metadata.description,
      profileId: `${scene.toLowerCase().replace("sponzaivy", "sponza-ivy")}-visbuf`,
      source: {
        hardwareId: CURRENT_DATASET_ID,
        referenceRenderer: "DonutRasterDebugReference",
        referenceRendererVariant: 14,
        visbufRenderer: "DonutVisDebug",
        visbufRendererVariant: 13,
        width: Number(record.width),
        height: Number(record.height),
        deployWidth: 720,
        deployHeight: 405,
        captureStride: Number(record.capture_stride),
      },
      frames: [],
    };
    group.frames.push(record);
    grouped.set(id, group);
  }

  const sequences = [];
  for (const group of [...grouped.values()].sort(
    (left, right) =>
      left.scene.localeCompare(right.scene) || left.order - right.order,
  )) {
    group.frames.sort(
      (left, right) =>
        Number(left.measurement_frame) - Number(right.measurement_frame),
    );
    const excludedBlankFrames = group.frames
      .filter((record) => Number(record.foreground_pixels_raster) === 0)
      .map((record) => Number(record.measurement_frame));
    const playableSourceFrames = group.frames.filter(
      (record) => Number(record.foreground_pixels_raster) > 0,
    );
    const targetDir = join(PUBLIC_VALIDATION, group.id);
    const comparisonDir = join(targetDir, "comparison");
    const differenceDir = join(targetDir, "difference");
    if (write) {
      await mkdir(comparisonDir, { recursive: true });
      await mkdir(differenceDir, { recursive: true });
    }

    const frameRecords = [];
    await mapWithConcurrency(playableSourceFrames, 4, async (record, index) => {
      const targetName = `${String(index).padStart(4, "0")}.webp`;
      const comparisonPath = join(comparisonDir, targetName);
      const differencePath = join(differenceDir, targetName);
      const rasterPath = join(BARYCENTRIC_ROOT, record.raster_path);
      const visbufPath = join(BARYCENTRIC_ROOT, record.visbuf_path);
      const heatmapPath = join(
        BARYCENTRIC_HEATMAPS,
        validationSceneFolder(record.scene),
        record.mode_name,
        `frame_${String(record.capture_index).padStart(6, "0")}.webp`,
      );

      if (write) {
        const [rasterFrame, visbufRightHalf] = await Promise.all([
          sharp(rasterPath, { failOn: "error" })
            .removeAlpha()
            .resize(720, 405, { fit: "fill" })
            .toBuffer(),
          sharp(visbufPath, { failOn: "error" })
            .removeAlpha()
            .resize(720, 405, { fit: "fill" })
            .extract({ left: 360, top: 0, width: 360, height: 405 })
            .toBuffer(),
        ]);
        await sharp(rasterFrame)
          .composite([{ input: visbufRightHalf, left: 360, top: 0 }])
          .webp({ quality: 82, effort: 6, smartSubsample: true })
          .toFile(comparisonPath);
        await sharp(heatmapPath, { failOn: "error" })
          .resize(720, 405, { fit: "fill" })
          .webp({ quality: 82, effort: 6, smartSubsample: true })
          .toFile(differencePath);
      }

      for (const path of [comparisonPath, differencePath]) {
        const metadata = await sharp(path).metadata();
        if (metadata.width !== 720 || metadata.height !== 405) {
          throw new Error(
            `${group.id}/${targetName}: expected 720x405, found ${metadata.width}x${metadata.height}.`,
          );
        }
      }
      const [
        comparisonStats,
        differenceStats,
        comparisonSha256,
        differenceSha256,
      ] = await Promise.all([
        stat(comparisonPath),
        stat(differencePath),
        sha256(comparisonPath),
        sha256(differencePath),
      ]);
      frameRecords[index] = {
        imageIndex: Number(record.capture_index),
        measurementFrame: Number(record.measurement_frame),
        comparisonSrc: `/data/validation/${group.id}/comparison/${targetName}`,
        differenceSrc: `/data/validation/${group.id}/difference/${targetName}`,
        comparisonBytes: comparisonStats.size,
        differenceBytes: differenceStats.size,
        comparisonSha256,
        differenceSha256,
        metrics: {
          interiorMaeLsb: Number(record.interior_mae_lsb),
          interiorP99Lsb: Number(record.interior_p99_lsb),
          interiorMaxLsb: Number(record.interior_max_lsb),
          interiorExactRatio: Number(record.interior_exact_ratio),
          coverageMismatchRatio: Number(record.coverage_mismatch_ratio),
        },
      };
    });

    const publicManifest = {
      id: group.id,
      scene: group.scene,
      mode: group.mode,
      modeName: group.modeName,
      label: group.label,
      shortLabel: group.shortLabel,
      order: group.order,
      description: group.description,
      profileId: group.profileId,
      source: group.source,
      validatedPairCount: group.frames.length,
      excludedBlankFrames,
      frameCount: frameRecords.length,
      frames: frameRecords,
    };
    if (write) {
      await writeFile(join(targetDir, "manifest.json"), json(publicManifest));
    }
    sequences.push(publicManifest);
  }
  return sequences;
}

async function softwareRasterTimeline() {
  const records = await readCsv(SOFTWARE_RASTER_FRAMES);
  const timeline = records
    .filter((record) => Number(record.measurement_frame) % 60 === 0)
    .map((record) => ({
      scene: validationSceneKey(record.scene),
      measurementFrame: Number(record.measurement_frame),
      triangleCount: Number(record.triangle_count),
      totalFragments: Number(record.total_fragments),
      coveredPixels: Number(record.covered_pixels),
      overdrawExtra: Number(record.overdraw_extra),
      averageOverdraw: Number(record.avg_overdraw),
      maximumOverdraw: Number(record.max_overdraw),
      rasterizedTriangles: Number(record.rasterized_triangles),
      skippedTriangles: Number(record.skipped_triangles),
      quadInstances: Number(record.quad_instances),
      quadCoveredLanes: Number(record.quad_covered_lanes),
      quadWasteLanes: Number(record.quad_waste_lanes),
      quadEfficiency: Number(record.quad_efficiency),
    }));
  if (timeline.length !== 176) {
    throw new Error(
      `Expected 176 software-raster profile-window rows, found ${timeline.length}.`,
    );
  }
  return timeline;
}

async function buildBundle({ write }) {
  const [normalized, campaignManifest, qualityReport, profiles] =
    await Promise.all([
      readCsv(join(CAMPAIGN_DATA, "all_results_normalized.csv")),
      readJson(join(CAMPAIGN_ROOT, "results", "_campaign_manifest.json")),
      readJson(join(CAMPAIGN_DATA, "quality_report.json")),
      cameraProfiles(),
    ]);
  const rows = resultRows(normalized);
  if (
    qualityReport.status !== "passed" ||
    rows.length !== 396 ||
    rows.some((row) => row.status !== "success")
  ) {
    throw new Error("The source campaign is not the validated 396-success dataset.");
  }

  const seenConditions = new Set();
  for (const row of rows) {
    const condition = JSON.stringify([
      row.config,
      row.scene,
      row.renderer,
      row.parameters,
      row.repeat,
    ]);
    if (seenConditions.has(condition)) {
      throw new Error(`Duplicate result condition detected: ${row.id}`);
    }
    seenConditions.add(condition);
  }

  const { sequences: primarySequences, captureReport } =
    await captureSequences({ write });
  const supplementalSequences = await supplementalCaptureSequences({ write });
  const sequences = [...primarySequences, ...supplementalSequences];
  const [comparison, validationSequences, rasterTimeline] = await Promise.all([
    hardwareComparison(rows),
    barycentricValidationSequences({ write }),
    softwareRasterTimeline(),
  ]);
  const bundle = {
    schemaVersion: 3,
    title: "TVB Performance Atlas",
    subtitle: "Visibility-buffer evidence, frame by frame",
    provenance: {
      campaignStatus: campaignManifest.campaign_status,
      campaignStartedAt: campaignManifest.started_at,
      campaignFinishedAt: campaignManifest.finished_at,
      captureFinishedAt: captureReport.finished_at,
      campaignExpectedRuns: campaignManifest.expected_runs,
      campaignSuccessfulRuns: campaignManifest.successful_runs,
      campaignSalvagedRuns: campaignManifest.salvaged_runs,
      campaignFailedRuns: campaignManifest.failed_runs,
      campaignSkippedRuns: campaignManifest.skipped_runs,
      sourceBranch: "codex/manual-material-experiments-20260729-200910",
      baseCommit: "c2c1257",
    },
    hardware: HARDWARE,
    datasets: DATASETS,
    summary: {
      resultRows: rows.length,
      captureSequences: sequences.length,
      captureFrames: sequences.reduce(
        (sum, sequence) => sum + sequence.source.capturedFrames,
        0,
      ),
      playableFrames: sequences.reduce(
        (sum, sequence) => sum + sequence.frameCount,
        0,
      ),
      validationSequences: validationSequences.length,
      validationPairs: 910,
      playableValidationPairs: validationSequences.reduce(
        (sum, sequence) => sum + sequence.frameCount,
        0,
      ),
      scenes: [...new Set(rows.map((row) => row.scene))],
      renderers: [...new Set(rows.map((row) => row.renderer))],
      experiments: Object.keys(EXPERIMENT_META).length,
    },
    experiments: experimentCatalog(rows),
    results: rows,
    profiles,
    sequences,
    validationSequences,
    rasterTimeline,
    hardwareComparison: comparison,
  };
  if (write) {
    await mkdir(PUBLIC_DATA, { recursive: true });
    await writeFileWithRetry(DASHBOARD_JSON, json(bundle));
  }
  return bundle;
}

async function verifyBundle() {
  if (!(await exists(DASHBOARD_JSON))) {
    throw new Error("dashboard.json does not exist; run npm run data:sync first.");
  }
  const committed = await readJson(DASHBOARD_JSON);

  const sourceDataset = join(CAMPAIGN_DATA, "all_results_normalized.csv");
  if (await exists(sourceDataset)) {
    const source = await buildBundle({ write: false });
    const committedShape = JSON.stringify(committed);
    const sourceShape = JSON.stringify(source);
    if (committedShape !== sourceShape) {
      throw new Error("Deployable dashboard data is stale relative to its sources.");
    }
  } else {
    await verifyDeployableBundle(committed);
  }
  return committed;
}

function deployablePath(src) {
  const publicRoot = resolve(PROJECT_ROOT, "public");
  const path = resolve(publicRoot, src.replace(/^\/+/, ""));
  const childPath = relative(publicRoot, path);
  if (childPath.startsWith("..") || isAbsolute(childPath)) {
    throw new Error(`Deployable path escapes public/: ${src}`);
  }
  return path;
}

async function verifyDeployableBundle(bundle) {
  if (
    bundle.schemaVersion !== 3 ||
    bundle.provenance?.campaignStatus !== "completed" ||
    bundle.provenance?.campaignExpectedRuns !==
      bundle.provenance?.campaignSuccessfulRuns ||
    bundle.provenance?.campaignSalvagedRuns !== 0 ||
    bundle.provenance?.campaignFailedRuns !== 0 ||
    bundle.provenance?.campaignSkippedRuns !== 0
  ) {
    throw new Error("Deployable dashboard campaign provenance is invalid.");
  }
  if (
    bundle.results?.length !== bundle.summary?.resultRows ||
    bundle.sequences?.length !== bundle.summary?.captureSequences ||
    bundle.validationSequences?.length !==
      bundle.summary?.validationSequences ||
    bundle.rasterTimeline?.length !== 176
  ) {
    throw new Error("Deployable dashboard summary does not match its data.");
  }

  const frameChecks = [
    ...bundle.sequences.flatMap((sequence) =>
      sequence.frames.map((frame) => [frame.src, frame.sha256]),
    ),
    ...bundle.validationSequences.flatMap((sequence) =>
      sequence.frames.flatMap((frame) => [
        [frame.comparisonSrc, frame.comparisonSha256],
        [frame.differenceSrc, frame.differenceSha256],
      ]),
    ),
  ];
  for (const [src, expectedHash] of frameChecks) {
    const path = deployablePath(src);
    if (!(await exists(path))) {
      throw new Error(`Deployable frame is missing: ${src}`);
    }
    if ((await sha256(path)) !== expectedHash) {
      throw new Error(`Deployable frame hash mismatch: ${src}`);
    }
  }
}

const verifyOnly = process.argv.includes("--verify");
const bundle = verifyOnly
  ? await verifyBundle()
  : await buildBundle({ write: true });
const deployableBytes = (
  await Promise.all(
    [
      ...bundle.sequences.flatMap((sequence) =>
        sequence.frames.map((frame) => frame.src),
      ),
      ...bundle.validationSequences.flatMap((sequence) =>
        sequence.frames.flatMap((frame) => [
          frame.comparisonSrc,
          frame.differenceSrc,
        ]),
      ),
    ].map(
      async (src) =>
        (await stat(join(PROJECT_ROOT, "public", src.replace(/^\/+/, "")))).size,
    ),
  )
).reduce((sum, size) => sum + size, 0);
console.log(
  `${verifyOnly ? "Verified" : "Generated"} ${bundle.summary.resultRows} results, ` +
    `${bundle.summary.playableFrames}/${bundle.summary.captureFrames} playable/captured frames ` +
    `across ${bundle.summary.captureSequences} beauty/debug sequences and ` +
    `${bundle.summary.validationPairs} raster/VisBuf validation pairs ` +
    `(${deployableBytes} deployable bytes).`,
);
