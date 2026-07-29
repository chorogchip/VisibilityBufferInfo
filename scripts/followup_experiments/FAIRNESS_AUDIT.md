# DeferredPrepass / VisBuf fairness audit

Hardware for the new campaigns in this directory is **NVIDIA GeForce RTX
5060 Ti 16GB**. Earlier archived campaigns under `datas/` were measured on an
**NVIDIA GeForce RTX 5070** and are not pooled with these measurements.

## Comparison pair

- Renderer 8: Donut Deferred with depth pre-pass
- Renderer 9: Donut visibility buffer with compute G-buffer
- Both use the same scene import, compacted draw stream, camera, resolution,
  texture-loading setting, VFC setting, warm-up, measurement length, and seed.
- Only functional render passes are timed. No clear-only timing or PSO-count
  metric was added.

## Fairness corrections

1. The visibility and depth pre-passes are opaque-only. They fetch position and
   instance data but do not fetch material constants, UVs, textures, or sample
   alpha. The benchmark no longer pays an alpha-test texture cost before
   material shading.
2. Deferred creates the same number of generic G-buffer PSOs as the active
   material-class count and selects them using the existing
   `virtual_shader_id`. Every PSO uses the same generic VS/PS bytecode; this
   matches VisBuf's class/PSO scheduling count without introducing compile-time
   material specialization.
3. Synthetic `scene_variant=1` now treats `material_count` and
   `material_assign_max_open` as independent axes. Existing top-level synthetic
   specs explicitly preserve their former material counts.
4. `donut_linear_gbuffer` adds a control ABI:
   - `false`: end-to-end renderer behavior. Deferred uses sRGB RTV/ROP encoding;
     VisBuf performs shader `LinearToSrgb` and writes UNORM UAVs.
   - `true`: both renderers use linear G-buffer channels, removing the
     sRGB-encoding-path difference when isolating reconstruction and scheduling.

## Differences intentionally retained

These are the algorithms being measured, not setup mismatches:

- VisBuf visibility raster, histogram, prefix, flatten/reorder, per-class
  indirect dispatch, attribute reconstruction, and UAV writes.
- Deferred depth pre-pass, generic raster G-buffer pass, RTV/ROP writes, and
  per-draw descriptor/material updates.
- End-to-end sRGB mode retains the real RTV/ROP versus UAV behavior. The linear
  control is reported separately rather than replacing it.

## Smoke evidence

- `00_renderer_fairness_smoke.json`: 4/4 successful runs for renderer 8/9 and
  sRGB/linear control on Sponza.
- `01_synth_decoupling_smoke.json`: 6/6 successful runs covering one material /
  one class, 64 materials / one class, and 64 materials / eight classes.
- Both reports contain zero salvaged, failed, and skipped runs. Normal stderr
  diagnostics remain in `runner_stderr`; successful rows have empty
  `runner_error`.
