# Frame capture and dashboard plan

## Product goal

The dashboard combines the completed material experiment campaign with
representative camera-path imagery. A viewer can scrub or autoplay the rendered
path, switch between normal and visibility-buffer debug views, and compare the
same scene against measured renderer timing, distribution, and pass breakdown
data.

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

Sponza Ivy is not selected for a new sequence: the completed campaign found
that the existing camera path did not reveal a useful visible difference from
base Sponza. Synthetic cases are static rather than camera-work experiments and
are represented by the already captured reference frame.

## Data contract

`scripts/sync-data.mjs` creates the deployable bundle:

- `public/data/dashboard.json`: campaign provenance, hardware, parameterized
  results, pass timings, experiment summaries, and capture sequence metadata.
- `public/data/captures/<sequence>/*.webp`: deterministic, web-sized copies of
  captured PNG frames.
- `public/data/captures/<sequence>/manifest.json`: image index to measurement
  frame mapping and source provenance.

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
- Scene and view selectors keep the closest normalized camera position when
  changing sequence.
- Normal, geometry-instance, primitive, combined-ID, and barycentric views.
- Campaign filters for experiment, scene, and renderer.
- Renderer comparison, percentile distribution, pass breakdown, and scaling
  charts use measured rows only.
- A hardware-provenance view compares only matched camera baselines and labels
  RTX 5060 Ti 16GB (today) separately from RTX 5070 (previous).
- Hardware and run-condition panels distinguish static machine capabilities
  from measured timing data.
- Missing or unavailable views are disclosed rather than synthesized.

## Acceptance criteria

1. Each capture run has a zero process exit, valid ProgramResult row, capture
   manifest, and the exact expected frame count.
2. Every deployable frame resolves from its manifest and has the declared
   dimensions.
3. Dashboard result counts equal the source campaign: 396 success, no duplicate
   run conditions, and no failed/skipped rows treated as measurements.
4. Frame scrubbing, autoplay, loop, scene/view switching, filters, and chart
   tooltips work with keyboard and pointer input.
5. A clean data sync and production build reproduce the committed bundle.
