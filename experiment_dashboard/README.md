# TVB Performance Atlas

TVB Performance Atlas is a self-contained interactive view of the
VisibilityBufferInfo material campaign. It synchronizes camera-path captures
with the original 60-frame playback profile windows, then exposes the complete
396-row experiment set through renderer, scene, percentile, scaling, and pass
timing views.

Hardware provenance is explicit and never pooled silently:

- The 2026-07-29 396-run campaign, synchronized profiles, and captures were
  measured on an NVIDIA GeForce RTX 5060 Ti 16GB.
- The earlier archived Sponza, Bistro, and Sponza + Ivy baseline results were
  measured on an NVIDIA GeForce RTX 5070. The atlas applies this
  owner-confirmed attribution instead of trusting inherited GPU labels in those
  summary CSV files.

The deployed site does not need Direct3D 12 or local scene assets. Its data and
960x540 WebP frames are generated ahead of time and checked into the project.

## Features

- Autoplay on load, infinite loop, play/pause, previous/next, slider scrubbing,
  2/4/8/12 fps choices, and keyboard controls.
- Sponza and Bistro PBR sequences.
- Geometry-instance, primitive, combined geometry/primitive, and barycentric
  visibility debug sequences.
- A vertical marker that follows the selected frame across the measured camera
  timing profile.
- Current GPU total, pass timings, and visible index count.
- All 18 material experiments with scene, renderer, and percentile filters.
- A matched-condition RTX 5060 Ti 16GB / RTX 5070 camera-baseline chart with
  separate series and cross-revision/profile-window comparison warnings.
- Run ledger, renderer aggregates, pass breakdowns, and source hardware.
- Responsive UI, accessible labels, reduced-motion handling, and social card.

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
- archived `datas/experiments/succeed/ex12`, `ex13`, and `ex14` camera summary
  CSVs for the RTX 5070 comparison subset

It regenerates `public/data/dashboard.json` and
`public/data/captures/**`. A second `data:sync` must produce identical hashes.

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
