import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { access, readFile } from "node:fs/promises";
import { join } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

const publicRoot = fileURLToPath(new URL("../public/", import.meta.url));

async function render() {
  const workerUrl = new URL("../dist/server/index.js", import.meta.url);
  workerUrl.searchParams.set("test", `${process.pid}-${Date.now()}`);
  const { default: worker } = await import(workerUrl.href);
  return worker.fetch(
    new Request("http://localhost/", {
      headers: { accept: "text/html" },
    }),
    {
      ASSETS: {
        fetch: async () => new Response("Not found", { status: 404 }),
      },
    },
    {
      waitUntil() {},
      passThroughOnException() {},
    },
  );
}

async function sha256(path) {
  const digest = createHash("sha256");
  digest.update(await readFile(path));
  return digest.digest("hex");
}

test("server renders the finished atlas shell", async () => {
  const response = await render();
  assert.equal(response.status, 200);
  assert.match(response.headers.get("content-type") ?? "", /^text\/html\b/i);

  const html = await response.text();
  assert.match(html, /<title>TVB Performance Atlas<\/title>/i);
  assert.match(html, /Loading the evidence bundle/);
  assert.match(html, /Preparing 396 benchmark results/);
  assert.doesNotMatch(html, /codex-preview|Your site is taking shape/i);
  assert.doesNotMatch(html, /react-loading-skeleton/i);
});

test("dashboard bundle preserves campaign and capture invariants", async () => {
  const dashboard = JSON.parse(
    await readFile(new URL("../public/data/dashboard.json", import.meta.url)),
  );

  assert.equal(dashboard.provenance.campaignStatus, "completed");
  assert.equal(dashboard.provenance.campaignExpectedRuns, 396);
  assert.equal(dashboard.provenance.campaignSuccessfulRuns, 396);
  assert.equal(dashboard.provenance.campaignSalvagedRuns, 0);
  assert.equal(dashboard.provenance.campaignFailedRuns, 0);
  assert.equal(dashboard.provenance.campaignSkippedRuns, 0);
  assert.equal(dashboard.schemaVersion, 2);
  assert.equal(dashboard.hardware.id, "manual-20260729-rtx5060ti16");
  assert.equal(
    dashboard.hardware.gpu.name,
    "NVIDIA GeForce RTX 5060 Ti 16GB",
  );
  assert.deepEqual(
    dashboard.datasets.map((dataset) => dataset.id),
    ["manual-20260729-rtx5060ti16", "historical-rtx5070"],
  );
  assert.equal(dashboard.results.length, 396);
  assert.ok(dashboard.results.every((row) => row.status === "success"));
  assert.ok(
    dashboard.results.every(
      (row) => row.hardwareId === "manual-20260729-rtx5060ti16",
    ),
  );
  assert.equal(new Set(dashboard.results.map((row) => row.id)).size, 396);
  assert.equal(dashboard.hardwareComparison.length, 12);
  assert.equal(
    dashboard.hardwareComparison.filter(
      (row) => row.datasetId === "manual-20260729-rtx5060ti16",
    ).length,
    6,
  );
  assert.equal(
    dashboard.hardwareComparison.filter(
      (row) => row.datasetId === "historical-rtx5070",
    ).length,
    6,
  );

  assert.equal(dashboard.sequences.length, 7);
  assert.equal(dashboard.summary.captureFrames, 348);
  assert.equal(dashboard.summary.playableFrames, 331);
  assert.deepEqual(
    dashboard.sequences.map((sequence) => sequence.frameCount),
    [39, 91, 38, 39, 39, 39, 46],
  );
  assert.deepEqual(
    dashboard.sequences.flatMap((sequence) => sequence.excludedBlankFrames),
    [
      39, 40, 41,
      91,
      15, 39, 40, 41,
      39, 40, 41,
      39, 40, 41,
      39, 40, 41,
    ],
  );
  assert.equal(dashboard.profiles.length, 4);
  assert.deepEqual(
    dashboard.profiles.map((profile) => profile.samples.length),
    [42, 42, 92, 92],
  );
});

test("every deployable frame matches its manifest digest", async () => {
  const dashboard = JSON.parse(
    await readFile(new URL("../public/data/dashboard.json", import.meta.url)),
  );

  for (const sequence of dashboard.sequences) {
    const manifestPath = new URL(
      `../public/data/captures/${sequence.id}/manifest.json`,
      import.meta.url,
    );
    const manifest = JSON.parse(await readFile(manifestPath));
    assert.equal(manifest.frameCount, sequence.frameCount);
    assert.deepEqual(manifest.frames, sequence.frames);

    for (const frame of sequence.frames) {
      const path = join(
        publicRoot,
        frame.src.replace(/^\/+/, ""),
      );
      await access(path);
      assert.equal(await sha256(path), frame.sha256);
    }
  }
});

test("interactive controls and debug disclosure remain in source", async () => {
  const source = await readFile(
    new URL("../app/AtlasApp.tsx", import.meta.url),
    "utf8",
  );
  for (const contract of [
    'aria-label="Previous frame"',
    'aria-label="Next frame"',
    'aria-label="Select frame"',
    'aria-label="Capture scene"',
    'aria-label="Capture output view"',
    'aria-label="Playback speed"',
    "Infinite loop",
    "Debug mode",
    "not the debug renderer's capture overhead",
    "GPU provenance",
    "RTX 5060 Ti 16GB",
    "RTX 5070",
    "never pooled across GPUs",
  ]) {
    assert.match(source, new RegExp(contract.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")));
  }
});
