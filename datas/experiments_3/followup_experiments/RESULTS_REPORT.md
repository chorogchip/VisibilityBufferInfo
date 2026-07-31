# Fair Donut material experiment: consolidated report

## Scope and provenance

- Branch: `codex/manual-material-experiments-20260729-200910`
- Master base: `c2c12571d344b492d2b7571f11018ff46c420285`
- Follow-up campaign: 2026-07-30 00:55–03:41 KST
- Current measurement GPU: **NVIDIA GeForce RTX 5060 Ti 16GB**
- Earlier archived `datas/` GPU: **NVIDIA GeForce RTX 5070**
- Build: `x64-Release`, Ninja single-config Release, MSVC 2026
- Compared performance renderers: Donut Deferred with depth pre-pass
  (variant 8) and Donut VisBuf + G-buffer (variant 9)

The RTX 5060 Ti and RTX 5070 data are labeled separately. Their absolute
timings are not pooled, averaged, or used as if they came from one hardware
population. The follow-up conclusions below use only the new RTX 5060 Ti
campaign unless an archived result is explicitly named.

No `automation/run-all-*` branch was touched. Neither
`safe_run_all_and_push.ps1` nor the full `run_all.ps1` path was used. Every
top-level runnable JSON was invoked explicitly and sequentially.

## Renderer fairness changes

The comparison was corrected in the existing renderer path; no alternate
scene variant or compile-time material specialization was introduced.

1. Donut Deferred now creates the same number of generic G-buffer PSOs as the
   active material-class count and selects them using the existing
   `virtual_shader_id`. The PSOs intentionally share the same generic shader
   body. This matches class/PSO switching opportunity without inventing a
   specialization benefit that VisBuf does not currently have.
2. Opaque visibility and depth passes no longer sample an alpha texture or
   read material/UV data. The depth pre-pass is vertex-only. Alpha clipping
   was also removed from the generic Deferred and VisBuf G-buffer paths so the
   measured contract is consistently opaque.
3. The existing synthetic material-grid scene now treats `material_count` as
   the number of materials and `material_assign_max_open` as the number of
   active classes. The two axes can be measured independently.
4. `donut_linear_gbuffer` selects a linear G-buffer control. The default path
   retains the end-to-end sRGB ABI, including shader-side VisBuf conversion;
   the control removes that ABI difference to isolate reconstruction and
   scheduling more closely.
5. No PSO-count metric was added. No clear operation is reported as a
   separate pass. No new per-frame performance stream was added; full-camera
   plots reuse the existing 60-frame playback profile sidecars.

The detailed source audit is in `FAIRNESS_AUDIT.md`.

## Campaign outcome

| Config | Purpose | Expected | Success | Salvaged | Failed | Skipped |
|---|---|---:|---:|---:|---:|---:|
| `00_renderer_fairness_smoke.json` | Real-scene renderer smoke | 4 | 4 | 0 | 0 | 0 |
| `01_synth_decoupling_smoke.json` | Material/class decoupling smoke | 6 | 6 | 0 | 0 | 0 |
| `02_synth_material_count_same_class_dense.json` | Material count, one class | 78 | 78 | 0 | 0 | 0 |
| `03_synth_class_count_fixed_materials_dense.json` | Class count, 255 materials | 78 | 78 | 0 | 0 | 0 |
| `04_synth_locality_dense.json` | Dense locality | 78 | 78 | 0 | 0 | 0 |
| `05_synth_diversity_dense.json` | Dense diversity at two locality extremes | 156 | 156 | 0 | 0 | 0 |
| `06_synth_locality_diversity_phase_dense.json` | Direct 9×9 locality/diversity map | 324 | 324 | 0 | 0 | 0 |
| `07_synth_material_class_matrix.json` | Independent material × class matrix | 180 | 180 | 0 | 0 | 0 |
| `08_synth_workload_scaling_dense.json` | Geometry workload scaling | 108 | 108 | 0 | 0 | 0 |
| `09_synth_resolution_scaling_dense.json` | Resolution scaling | 42 | 42 | 0 | 0 | 0 |
| `10_synth_seed_robustness_dense.json` | 20 paired seeds | 40 | 40 | 0 | 0 | 0 |
| `11_synth_linear_gbuffer_control.json` | sRGB ABI versus linear control | 48 | 48 | 0 | 0 | 0 |
| `12_real_class_count_dense.json` | Real-scene class count | 42 | 42 | 0 | 0 | 0 |
| `13_real_diversity_dense.json` | Real-scene diversity | 78 | 78 | 0 | 0 | 0 |
| `14_real_full_camera_linear_control.json` | Complete cameras and pass profiles | 12 | 12 | 0 | 0 | 0 |
| `15_real_texture_vfc_ablation.json` | Texture/VFC ablation | 24 | 24 | 0 | 0 | 0 |
| `16_real_software_raster_reference.json` | Full-camera workload proxies | 3 | 3 | 0 | 0 | 0 |
| **Total** |  | **1,301** | **1,301** | **0** | **0** | **0** |

The mandatory synthetic smoke was exactly `success=6`, `salvaged=0`,
`failed=0`. Ivy was present at
`assets/scenes/unpacked/main_sponza_ivy/NewSponza_Main_Ivy_glTF.gltf`, so no
sample required missing-asset substitution or skipping.

## Main findings

### Material records are not the class-scheduling cost

With every material mapped to one generic class, increasing material count
from 1 to 255 changed Deferred from 0.24254 to 0.25240 ms and VisBuf from
0.31766 to 0.32868 ms. The paired Deferred/VisBuf ratio stayed within
0.7635–0.7731. Material-record count alone therefore has only a small effect
in this synthetic case and does not explain the VisBuf gap.

With material count fixed at 255, increasing active classes from 1 to 255
left Deferred nearly flat (0.28109 to 0.27960 ms) while VisBuf increased from
0.34434 to 0.56192 ms, a 63.2% increase. The ratio fell from 0.8163 to
0.4976. Because both paths use generic shader bodies and Deferred receives
the matched PSO selection opportunity, this is evidence for the current
VisBuf bin/dispatch path's per-class cost, not evidence for static material
specialization.

The independent material × class matrix had 45 valid sampled cells. Every
cell favored Deferred; ratios ranged from 0.5136 to 0.8237.

### Locality, diversity, seed, workload, and resolution

- At 255 materials and 64 classes, locality 0→1 reduced Deferred from
  0.29266 to 0.25513 ms and VisBuf from 0.41457 to 0.38594 ms. Both benefited,
  but Deferred benefited more in this setup.
- The direct 9×9 locality/diversity phase map contained 81 measured cells.
  Ratios were 0.6512–0.7246; there was no equal-time crossover.
- Across 20 paired seeds, the mean Deferred/VisBuf ratio was 0.6886 with
  sample standard deviation 0.0117 and range 0.6632–0.7102. The ordering is
  robust to the sampled assignment seeds.
- Increasing `geometry_div` increases synthetic triangle density. At
  `geometry_div=256`, locality 0 reached the closest sampled workload point,
  ratio 0.9160. The fixed VisBuf overhead is increasingly amortized by heavy
  geometry, but no tested workload crossed parity.
- Across 0.5184–8.2944 megapixels and three locality settings, ratios remained
  0.5764–0.7335. Resolution scaling alone did not create a VisBuf win.

### Complete real-scene cameras

End-to-end sRGB results:

| Scene | DeferredPrepass avg (ms) | VisBuf avg (ms) | Deferred / VisBuf | VisBuf extra time |
|---|---:|---:|---:|---:|
| Sponza | 1.17241 | 1.76071 | 0.6659 | +50.2% |
| Sponza Ivy | 1.68781 | 2.07718 | 0.8125 | +23.1% |
| Bistro | 0.53941 | 0.66268 | 0.8140 | +22.9% |

The result is scene dependent, but DeferredPrepass wins all three complete
camera paths on this implementation and GPU.

The pass breakdown explains why:

- Sponza: Deferred geometry + depth is 1.02844 ms. VisBuf compute G-buffer
  alone is 1.31895 ms, then visibility is 0.22150 ms and
  histogram/prefix/flatten add 0.06713 ms.
- Sponza Ivy: VisBuf compute G-buffer (1.31592 ms) is cheaper than Deferred
  geometry + depth (1.54054 ms), but visibility (0.53925 ms) plus binning
  (0.06990 ms) reverses the saving.
- Bistro: VisBuf compute G-buffer (0.21666 ms) saves 0.17530 ms relative to
  Deferred geometry + depth (0.39196 ms), but visibility (0.21425 ms) and
  binning (0.07094 ms) more than consume the saving.

Lighting and tonemap are close between renderers. The actionable costs are
therefore visibility, class scheduling, and compute G-buffer
reconstruction/access—not the shared lighting pass. Pass plots follow
execution order and use related blue colors for depth pre-pass and
visibility. Clear operations are excluded from the pass stack.

### sRGB ABI control

Switching to a linear G-buffer changed complete-camera averages by less than
0.8% for every scene/renderer pair. Synthetic paired ratios moved by at most
0.0069. The sRGB UAV conversion/resource-ABI distinction is real, but it is
not the primary explanation for the observed renderer ordering on this GPU.

### Real-scene class/diversity and ablation

The 600-frame real-scene class and diversity sweeps were non-monotonic. No
sample produced a robust VisBuf win. A few points approached parity:
Sponza class 64 reached ratio 0.9797, and Sponza Ivy diversity 0.95 reached
0.9908. These are useful hypotheses for longer replicated runs, not
crossovers.

In the texture/VFC ablation, Deferred won 11 of 12 scene/condition pairs. The
single exception was Sponza Ivy with textures off and VFC off, ratio 1.0452.
Enabling textures materially increased both renderer costs. VFC reduced
geometry work, but its effect on the renderer ratio depended on scene and
texture state.

### Software-raster workload proxies

Full-camera software-raster output was collected for all three scenes:

| Scene | Avg triangles | Avg fragments | Avg overdraw | Avg quad efficiency |
|---|---:|---:|---:|---:|
| Sponza | 1,624,252 | 5,073,254 | 2.859 | 0.696 |
| Sponza Ivy | 5,058,509 | 5,158,902 | 2.904 | 0.683 |
| Bistro | 1,032,518 | 8,868,774 | 4.785 | 0.758 |

Bistro has fewer average triangles than either Sponza case but much higher
fragment work and overdraw. Across aligned 60-frame windows, triangle count
and quad-waste lanes correlate strongly with total time in most
scene/renderer pairs; the exact coefficients are in
`plots/data/19_raster_gpu_correlations.csv`. These are software-derived
workload proxies and within-camera correlations, not direct hardware counter
samples or proof of causality. Legitimate blank-view frames remain in the raw
tables and are never replaced with fabricated values.

## Visual correctness evidence

The already completed barycentric/derivative campaign was not rerun:

- 910 raster-reference/VisBuf capture pairs across Sponza, Sponza Ivy, and
  Bistro
- mean interior MAE 0.018412 8-bit LSB
- 98.3831% bit-exact interior channels
- 0.000000% mean coverage mismatch

Debug-view timings are not interpreted as performance. The full report is
`scripts/barycentric_validation/analysis/README.md`.

The earlier representative camera capture campaign also remains preserved.
The interactive JavaScript visualization requested later was intentionally
postponed at the user's direction and was not mixed into this renderer/data
change.

## Quality and reproducibility

`audit_results.py` reran the existing campaign validators over every output:

- 1,301 expected runs and 1,301 consolidated rows
- 1,301 success, 0 salvaged, 0 failed, 0 skipped
- no duplicate or missing run index/condition
- no `runner_error` content on a success row
- required ProgramResult fields and numeric timings present
- parameters retained in the result rows
- no result backup inside the active results tree

Machine-readable result: `results/_quality_report.json`.

`verify_plots.py` rebuilt the plot tree twice. All 86 compared PNG, SVG,
CSV, index, and README files had identical SHA-256 hashes. Machine-readable
result: `plots/data/reproducibility_report.json`.

## Plot outputs

- Generator: `plot_results.py`
- Reproducibility check: `verify_plots.py`
- Pinned Python packages: `plot_requirements.txt`
- PNG: `plots/png/` (24)
- SVG: `plots/svg/` (24)
- Plot-ready CSV: `plots/data/`
- Illustrated index and input/filter documentation: `plots/README.md`

The plots adapt the earlier successful experiment scripts: pass timelines and
median pass stacks from ex10, software-raster/profile alignment from ex10 and
ex12–18, and direct sampled heatmap/crossover presentation from ex5/ex6.
Every plot names the RTX 5060 Ti 16GB source and separately identifies the
RTX 5070 archive.

## Limitations and next work

1. The current material classes do not have statically specialized shader
   bodies. The present result measures generic class/bin scheduling; it must
   not be described as the performance of a specialized VisBuf design.
2. Complete-camera conditions have many temporal samples but one process run
   per scene/renderer/ABI condition. Replicated complete-camera runs would
   quantify run-to-run variance.
3. The closest synthetic workload point is still below parity. A targeted
   geometry-density extension above `geometry_div=256` can locate whether a
   crossover exists, while retaining the same 600-frame protocol.
4. Direct Nsight/PIX hardware counters were not captured. The repository has
   reproducible representative cases, and the software-raster proxies now
   identify which camera windows should be profiled.
5. The opaque fairness contract intentionally excludes alpha test. A future
   cutout benchmark should be a separately named renderer contract rather than
   silently reintroducing texture sampling into the visibility pass.
6. Cross-GPU comparison requires rerunning the same corrected specs on the
   RTX 5070. The historical archive should remain separate until then.
7. The current Ivy camera path is valid for performance, but earlier capture
   QA found little visible ivy distinction in representative frames. A
   presentation camera should deliberately expose ivy geometry without
   replacing the performance camera.

The central conclusion is narrow but clear: on the corrected, generic,
opaque Donut paths measured on the RTX 5060 Ti 16GB, VisBuf's compute
G-buffer savings are real in Ivy and Bistro, yet visibility and per-class
bin/dispatch costs consume those savings. Material count by itself is not the
problem; active class scheduling and the compute reconstruction path are the
next optimization targets.
