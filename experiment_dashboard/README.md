# TVB Performance Atlas

TVB Performance Atlas is a self-contained interactive camera-path view of the
VisibilityBufferInfo evidence. The first viewport keeps three things visible at
once: normal PBR output, a selectable visibility/reconstruction debug output,
and the measured Camera Timeline.

Hardware provenance is explicit and never pooled silently:

- The 2026-07-29 396-run campaign, synchronized profiles, and captures were
  measured on an NVIDIA GeForce RTX 5060 Ti 16GB.
- The earlier archived Sponza, Bistro, and Sponza + Ivy baseline results were
  measured on an NVIDIA GeForce RTX 5070. The atlas applies this
  owner-confirmed attribution instead of trusting inherited GPU labels in those
  summary CSV files.

The deployed site does not need Direct3D 12 or local scene assets. Its data,
960x540 PBR/ID frames, and 720x405 validation frames are generated ahead of
time and checked into the project.

## Features

- Autoplay on load, infinite loop, play/pause, previous/next, slider scrubbing,
  2/4/8/12 fps choices, chart seeking, and keyboard controls.
- Side-by-side normal PBR and debug viewers for Sponza, Sponza + Ivy, and
  Bistro.
- Geometry-instance, primitive, and combined identity views where available.
- Seven raster-reference / VisBuf analytic reconstruction modes:
  linear/perspective barycentrics, barycentric `ddx`/`ddy`, UV `ddx`/`ddy`,
  and a texture LOD proxy.
- 910 frame pairs exposed as a center-split comparison or absolute pixel
  difference, with frame-local interior MAE, bit-exact ratio, and coverage
  mismatch.
- One Camera Timeline only: Deferred prepass and VisBuf totals plus every
  measured pass, individually toggleable and ordered by execution. Clear is
  intentionally excluded.
- Optional timeline workload trace and cursor readout for visible indices,
  input triangles, fragments, overdraw, and quad-lane efficiency.
- Explicit hardware provenance: current camera/performance data is RTX 5060 Ti
  16GB; the earlier RTX 5070 archive is labeled but never pooled into this
  timeline.
- Responsive UI, accessible controls, reduced-motion handling, and social card.

## Reproduce the data

From this directory:

```powershell
npm install
npm run capture:verify
npm run data:sync
npm run data:verify
npm test
```

`data:sync` reads:

- `../scripts/material_experiments/plots/data/all_results_normalized.csv`
- the full-camera profile sidecars from experiment `30`
- `capture_specs/results/dashboard_camera_capture`
- PBR anchor frames from material capture experiment `31`
- `../scripts/barycentric_validation/analysis/data/frame_metrics.csv`
- the corresponding local raster-reference and VisBuf validation captures
- `../scripts/followup_experiments/plots/data/18_software_raster_frames.csv`
- archived `datas/experiments/succeed/ex12`, `ex13`, and `ex14` camera summary
  CSVs for the RTX 5070 comparison subset

It regenerates `public/data/dashboard.json` and
`public/data/captures/**` plus `public/data/validation/**`. The validation
bundle stores a 720x405 center split (raster on the left, VisBuf on the right)
and an absolute-difference frame. A second `data:sync` must produce identical
hashes.

All 910 measured pairs remain counted in provenance. The 42 pairs from the
three terminal blank Sponza/Sponza + Ivy camera windows are recorded in
manifests but omitted from autoplay, leaving 868 useful comparison frames.

The 1.86GB full-resolution barycentric-validation source tree is deliberately
not committed. The optimized deployable frames, manifests, scalar error data,
and reproducible conversion script are committed.

## Re-run the renderer captures

Build the Release executable first, then run the capture-only spec explicitly:

```powershell
python ../scripts/run.py capture_specs/dashboard_camera_capture.json
npm run capture:sanitize
npm run data:sync
```

The seven capture runs preserve the full Sponza/Bistro camera measurement
windows. Raw 1280x720 PNGs stay local under `capture_specs/results` and are
ignored by Git. The deployable 960x540 WebP derivatives are committed.

See [CAPTURE_PLAN.md](CAPTURE_PLAN.md) for selection rationale, data contracts,
and acceptance criteria, and [CAPTURE_REPORT.md](CAPTURE_REPORT.md) for the
completed run audit.

## Stack

- React 19 and TypeScript
- vinext/Vite for a Cloudflare-compatible application build
- Recharts for measured data visualization
- Sharp for deterministic deployable frame generation
- Node's test runner for rendered HTML, data invariants, asset digests, and
  interaction contracts
