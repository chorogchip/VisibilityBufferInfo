# Manual material experiment campaign report

## Scope and provenance

- Branch: `codex/manual-material-experiments-20260729-200910`
- Base commit: `c2c1257`
- Campaign time: 2026-07-29 20:32–21:56 KST
- GPU: NVIDIA GeForce RTX 5060 Ti
- CPU: Intel Core Ultra 7 265KF (20 cores / 20 logical processors)
- OS: Windows 11 Home, build 26200
- Toolchain: Visual Studio 2026 18.8.1 / MSVC 14.51.36231
- Python: 3.14.0
- Build: `x64-Release`, Ninja single-config, `CMAKE_BUILD_TYPE=Release`

The campaign was created from the latest `master` available at start. No
`automation/run-all-*` branch was modified, merged, or deleted. Neither
`safe_run_all_and_push.ps1` nor `run_all.ps1` was used.

## Final run counts

| Config | Expected | Success | Salvaged | Failed | Skipped |
|---|---:|---:|---:|---:|---:|
| `00_smoke_synthetic_material_grid.json` | 6 | 6 | 0 | 0 | 0 |
| `01_smoke_real_pbr_feature.json` | 3 | 3 | 0 | 0 | 0 |
| `10_synth_locality_fixed64.json` | 15 | 15 | 0 | 0 | 0 |
| `11_synth_diversity_fixed64.json` | 30 | 30 | 0 | 0 | 0 |
| `12_synth_locality_diversity_map_fixed64.json` | 75 | 75 | 0 | 0 | 0 |
| `13_synth_open_bin_count_diagnostic.json` | 54 | 54 | 0 | 0 | 0 |
| `14_synth_workload_scaling.json` | 16 | 16 | 0 | 0 | 0 |
| `15_synth_seed_robustness.json` | 10 | 10 | 0 | 0 | 0 |
| `16_synth_renderer_compare_selected.json` | 12 | 12 | 0 | 0 | 0 |
| `20_real_random_bin_count_quick_all_scenes.json` | 45 | 45 | 0 | 0 | 0 |
| `21_real_random_diversity_quick_all_scenes.json` | 45 | 45 | 0 | 0 | 0 |
| `22_real_pbr_feature_renderer_compare_full.json` | 9 | 9 | 0 | 0 | 0 |
| `23_real_random_selected_renderer_compare_quick.json` | 18 | 18 | 0 | 0 | 0 |
| `24_real_resolution_scaling_pbr_quick.json` | 12 | 12 | 0 | 0 | 0 |
| `25_real_texture_vfc_ablation_quick.json` | 16 | 16 | 0 | 0 | 0 |
| `30_final_selected_full_camera.json` | 12 | 12 | 0 | 0 | 0 |
| `31_capture_representative_frames.json` | 8 | 8 | 0 | 0 | 0 |
| `32_nsight_representative_cases.json` | 10 | 10 | 0 | 0 | 0 |
| **Total** | **396** | **396** | **0** | **0** | **0** |

The mandatory synthetic smoke result was exactly `success=6`,
`salvaged=0`, `failed=0`, `skipped=0`.

## Correctness fixes

1. `scripts/run.py` now classifies a run as success only when the process
   returns zero and at least one structurally and numerically valid
   ProgramResult row is readable. Ordinary stderr diagnostics are preserved in
   `runner_stderr` but do not populate `runner_error`.
2. Nonzero exits with a valid row are `salvaged`; timeout, start failure,
   interrupted execution, unreadable/missing CSV, missing required fields, and
   renderer/device error diagnostics are `failed`.
3. JSON input supports UTF-8 and UTF-8 BOM.
4. Missing scene/camera assets are checked per sample and recorded as
   `skipped_missing_asset`, so other samples in the same JSON continue.
5. The campaign driver locks the sorted top-level runnable JSON list, updates
   `_campaign_manifest.json`, validates every config, and resumes without
   rerunning terminal configs.
6. Korean MSVC 2026 `/showIncludes` output was not decoded correctly by CMake,
   leaving Ninja with zero header dependencies and stale objects after
   `ProgramArgument` layout changes. CMake now sets the actual Korean prefix.
   A clean Release rebuild produced 23 tracked dependencies for
   `Application.cpp.obj`, including `ProgramArgument.h`.
7. Runner progress output is best-effort so a detached console cannot abort a
   still-running experiment. Seven focused unit tests cover the classification,
   BOM, CSV validation, device error, timeout, and detached-console policies.
8. Published result diagnostics use `${REPOSITORY_ROOT}` and `${RUN_TEMP}`
   tokens instead of machine-local user paths.

## Assets and camera QA

Ivy exists at:

`assets/scenes/unpacked/main_sponza_ivy/NewSponza_Main_Ivy_glTF.gltf`

It was restored to the applicable samples using repository-relative paths and
ran successfully; no Ivy skip was needed. The Ivy glTF has more nodes, meshes,
materials, images, and buffers than base Sponza, and its measured timings
differ. In the representative camera capture, however, 4 of 5 corresponding
Sponza/Ivy PNGs were byte-identical and the remaining sampled frame was
visually near-identical. The current camera samples therefore did not capture
visibly distinct ivy, which is retained as a presentation-quality warning
rather than hidden or filled with fabricated data.

The synthetic camera is `position z=-3`, look-at `(0,0,0)`, `z=[-1,1]`,
`xy_minmax=1`. A 1280×720 smoke capture showed the material grid centered,
large, and unclipped, so no JSON camera change was made.

## Quality audit

`plot_results.py --verify-only` passed all configs:

- 396 expected rows and 396 consolidated rows
- 396 success, 0 salvaged, 0 failed, 0 skipped
- no duplicate run index or full parameter condition
- no missing run index
- no success row with `runner_error`
- all required ProgramResult fields and positive total timing statistics
- all named passes have finite, nonnegative timings
- consistent per-config ProgramResult schemas
- report parameters retained in consolidated CSV columns
- no result backup mixed into the new `results` tree

The machine-readable audit is
`plots/data/quality_report.json`; the empty failed/skipped table is
`plots/data/failed_skipped_cases.csv`.

Two discarded attempts are not mixed into final data:

- The first synthetic smoke exposed a stale-object `ProgramArgument` ABI
  mismatch. The build dependency bug was fixed, the Release tree was cleaned
  and rebuilt, and the smoke config folder was replaced before rerunning.
- The first `11_synth_diversity_fixed64` attempt lost its output console after
  27 successes. Console-detach handling was fixed and that config folder was
  replaced before all 30 runs were rerun.

## Plot outputs

- Script: `plot_results.py`
- Requirements: `plot_requirements.txt`
- Plot index and conditions: `plots/data/plot_index.csv`
- Normalized plot data: `plots/data/all_results_normalized.csv`
- PNG: `plots/png/`
- SVG: `plots/svg/`
- Illustrated index: `plots/README.md`

There are 35 PNG and 35 SVG plots: one for each of the 18 configs plus
locality, diversity, locality/diversity heatmap, open-bin/class count, workload
scaling, seed robustness, renderer comparison, real bin count, real diversity,
resolution scaling, texture/VFC ablation, scene comparison, synthetic and
real-scene combined summaries, pass timing breakdown, total-time statistics,
and status summary.

The plot tree contains 77 generated files. A full second generation changed
zero SHA-256 hashes and removed zero files.

## Capture storage policy

`31_capture_representative_frames` produced 32 full-resolution PNGs totaling
84,422,312 bytes. All remain in the local result tree. Git includes the
capture metadata and a small representative subset; the remaining original
frames are deliberately left local to avoid adding the entire 84.4 MB capture
set to repository history. No capture was deleted.

## Reproduction

Run one config explicitly:

```powershell
python scripts/material_experiments/campaign.py run <config-name.json>
```

Validate the completed campaign:

```powershell
python scripts/material_experiments/campaign.py finalize
python scripts/material_experiments/plot_results.py --verify-only
```

Rebuild plots:

```powershell
python -m pip install -r scripts/material_experiments/plot_requirements.txt
python scripts/material_experiments/plot_results.py
```
