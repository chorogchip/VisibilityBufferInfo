# Frame capture and dashboard plan

## Product goal

The dashboard combines the completed camera-path measurements with normal,
visibility-ID, and raster-reference/VisBuf reconstruction imagery. A viewer can
scrub or autoplay the path while keeping the normal frame, selected debug
frame, and measured pass timeline visible in the same viewport.

The frame player is a visual explanation surface. Capture runs are not used as
replacement performance measurements; charts continue to use the validated
396-row campaign under `scripts/material_experiments/results`.

The 396-row campaign and every capture in this plan belong to the RTX 5060 Ti
16GB dataset. Earlier archived camera baselines belong to RTX 5070; the viewer
keeps the two datasets as separate series and does not pool their samples.

## Capture selection

| Sequence | Scene | Renderer | Debug mode | Frames | Stride | Reason |
|---|---|---|---:|---:|---:|---|
| Sponza PBR | Sponza | DonutVisGBuffer | n/a | 42 | 60 | Full presentation camera path |
| Bistro PBR | Bistro | DonutVisGBuffer | n/a | 92 | 60 | Longer real-scene camera path |
| Sponza geometry instances | Sponza | DonutVisDebug | 0 | 42 | 60 | Instance boundaries |
| Sponza primitives | Sponza | DonutVisDebug | 1 | 42 | Primitive-ID distribution |
| Sponza geometry + primitives | Sponza | DonutVisDebug | 2 | 42 | 60 | Combined visibility identity |
| Sponza barycentrics | Sponza | DonutVisDebug | 3 | 42 | 60 | Interpolation reconstruction |
| Bistro geometry + primitives | Bistro | DonutVisDebug | 2 | 46 | 120 | Debug-path validation on the longer real scene |

All sequences use the original camera CSVs through their final measurement
window. They retain the original 60 warm-up frames, scene textures, VFC, and
PBR material assignment. The capture-only resolution is 1280x720 because image
quality, not timing comparability, is the goal.

The existing material-campaign capture supplies five Sponza + Ivy PBR anchors.
The completed barycentric validation supplies 42 debug frames for each of seven
reconstruction modes. No renderer run is repeated for the dashboard.

## Reconstruction validation reuse

The already completed validation contributes 910 synchronized frame pairs:

| Scene | Frames per mode | Modes | Raster/VisBuf pairs |
|---|---:|---:|---:|
| Sponza | 42 | 7 | 294 |
| Sponza + Ivy | 42 | 7 | 294 |
| Bistro | 46 | 7 | 322 |

Each pair is preserved as a center-split raster-reference/VisBuf frame and an
absolute pixel-difference heatmap. The timeline continues to use the measured
DonutDeferredPrepass and DonutVisGBuffer profiles; debug-renderer execution
time is never presented as performance evidence.

The final three blank camera windows in each Sponza mode remain part of the 910
validated-pair provenance but are listed as excluded in the deployable
manifests. Autoplay therefore traverses 868 non-blank validation frames.

## Data contract

`scripts/sync-data.mjs` creates the deployable bundle:

- `public/data/dashboard.json`: campaign provenance, hardware, parameterized
  results, pass timings, experiment summaries, and capture sequence metadata.
- `public/data/captures/<sequence>/*.webp`: deterministic, web-sized copies of
  captured PNG frames.
- `public/data/captures/<sequence>/manifest.json`: image index to measurement
  frame mapping and source provenance.
- `public/data/validation/<scene-mode>/comparison/*.webp`: raster on the left,
  VisBuf on the right, synchronized by measurement frame.
- `public/data/validation/<scene-mode>/difference/*.webp`: per-frame absolute
  pixel difference.
- `rasterTimeline` in `dashboard.json`: matching software-raster triangle,
  fragment, overdraw, and quad-efficiency rows sampled at profile windows.

Raw PNGs remain under `capture_specs/results` and are excluded from Git. The
derived WebP files are committed so a clone can build and deploy without local
scene assets or Direct3D 12.

Frames with no meaningful RGB variation remain in the raw capture evidence.
They are explicitly listed in each deployable manifest and omitted from
autoplay, for both beauty and debug views. This includes terminal frames after
the path finishes and one mid-path geometry-instance frame that is a single
flat ID color. No measured timing row is removed.

## Viewer behavior

- Starts in autoplay and loops indefinitely.
- Play/pause, previous/next, direct frame slider, playback speed, and keyboard
  controls.
- Scene and debug selectors preserve normalized camera progress.
- Normal, geometry-instance, primitive, combined-ID, barycentric, derivative,
  UV-gradient, and LOD-proxy views.
- One Camera Timeline only. Deferred and VisBuf totals and every measured pass
  can be toggled independently; pass controls follow execution order.
- Depth prepass and visibility use related blue hues. Histogram/prefix/flatten
  share an orange family, compute G-buffer is purple, and common post passes
  are neutral. Clear is not plotted.
- One optional secondary workload trace and a cursor-local workload readout.
- Hardware provenance labels RTX 5060 Ti 16GB as current and RTX 5070 as the
  earlier archive. Samples are never pooled across GPUs.
- Missing or unavailable views are disclosed rather than synthesized.

## Acceptance criteria

1. Each capture run has a zero process exit, valid ProgramResult row, capture
   manifest, and the exact expected frame count.
2. Every deployable frame resolves from its manifest and has the declared
   dimensions.
3. Dashboard result counts equal the source campaign: 396 success, no duplicate
   run conditions, and no failed/skipped rows treated as measurements.
4. Frame scrubbing, autoplay, loop, scene/debug switching, pass toggles,
   timeline seeking, and chart tooltips work with keyboard and pointer input.
5. A clean data sync and production build reproduce the committed bundle.
