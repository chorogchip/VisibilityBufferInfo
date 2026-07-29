# Follow-up experiment plots

- Current measurement hardware: **NVIDIA GeForce RTX 5060 Ti 16GB**
- Earlier archived `datas/` results: **NVIDIA GeForce RTX 5070**
- The two hardware pools are labeled separately and are not numerically pooled.
- Only `runner_status=success` rows are used as measurements.
- Clear operations and PSO-count metrics are not included.
- Pass stack order and colors follow renderer execution order; depth pre-pass
  and visibility use related blue colors.

## Reproduction

```powershell
.\.venv\Scripts\python.exe -m pip install -r scripts\followup_experiments\plot_requirements.txt
.\.venv\Scripts\python.exe scripts\followup_experiments\plot_results.py
```

The script cleans only `scripts/followup_experiments/plots`, then rebuilds
PNG, SVG, plot-ready CSV tables, and this index.

## Reused early plotting ideas

- `ex10/.../03_pass_by_frame_plots.py`: pass timelines
- `ex10/.../04_median_pass_breakdown.py`: stacked pass breakdown
- `ex10/.../11_bistro_all_raster_stat_figures.py`: raster/profile alignment
- `ex12-18 scripts/plot_sponza_raster_metric_comparison.py`: correlation overview
- `ex5` and `ex6` orthogonal plot bundles: direct sampled heatmaps and
  equal-performance contours

## Plot index

| Plot | Description |
|---|---|
| [00_campaign_completion](png/00_campaign_completion.png) | Expected and successful runs per JSON |
| [01_synth_material_count_same_class](png/01_synth_material_count_same_class.png) | Synthetic material count with one shared generic class |
| [02_synth_class_count_fixed_materials](png/02_synth_class_count_fixed_materials.png) | Synthetic class / generic PSO count with 255 materials |
| [03_synth_locality_response](png/03_synth_locality_response.png) | Synthetic locality response (255 materials, 64 classes) |
| [04_synth_diversity_response](png/04_synth_diversity_response.png) | Diversity response at locality 0 and 1 |
| [05_synth_locality_diversity_phase](png/05_synth_locality_diversity_phase.png) | Direct sampled 9×9 phase map with equal-time contour |
| [06_synth_material_class_matrix](png/06_synth_material_class_matrix.png) | Independent material-count and class-count matrix |
| [07_synth_workload_scaling](png/07_synth_workload_scaling.png) | Synthetic workload scaling at three locality levels |
| [08_synth_resolution_scaling](png/08_synth_resolution_scaling.png) | Synthetic resolution scaling at three locality levels |
| [09_synth_seed_robustness](png/09_synth_seed_robustness.png) | Per-seed paired results and speed-ratio distribution |
| [10_synth_srgb_vs_linear_control](png/10_synth_srgb_vs_linear_control.png) | Synthetic end-to-end sRGB ABI versus linear G-buffer control |
| [11_real_scene_class_count](png/11_real_scene_class_count.png) | Real-scene generic class / PSO count response |
| [12_real_scene_diversity](png/12_real_scene_diversity.png) | Real-scene material-class diversity response |
| [13_full_camera_total_statistics](png/13_full_camera_total_statistics.png) | Average, median, P90, and P99 full-camera totals |
| [14_full_camera_pass_breakdown_srgb](png/14_full_camera_pass_breakdown_srgb.png) | Execution-order pass breakdown (srgb) |
| [14_full_camera_pass_breakdown_linear](png/14_full_camera_pass_breakdown_linear.png) | Execution-order pass breakdown (linear) |
| [15_full_camera_scene_comparison](png/15_full_camera_scene_comparison.png) | Full-camera renderer speed ratio by scene and G-buffer ABI |
| [16_timeline_sponza](png/16_timeline_sponza.png) | Sponza full-camera total and pass timelines |
| [16_timeline_sponza_ivy](png/16_timeline_sponza_ivy.png) | Sponza Ivy full-camera total and pass timelines |
| [16_timeline_bistro](png/16_timeline_bistro.png) | Bistro full-camera total and pass timelines |
| [17_real_texture_vfc_ablation](png/17_real_texture_vfc_ablation.png) | Texture-loading and view-frustum-culling ablation |
| [18_software_raster_scene_summary](png/18_software_raster_scene_summary.png) | Scene means for triangle, fragment, coverage, overdraw, and quad metrics |
| [19_raster_overdraw_vs_gpu_time](png/19_raster_overdraw_vs_gpu_time.png) | Aligned profile-window overdraw versus renderer total; color is quad efficiency |
| [20_raster_gpu_correlation_overview](png/20_raster_gpu_correlation_overview.png) | Pearson correlation between aligned software-raster metrics and total time |

## Failed and skipped cases

`data/failed_skipped_cases.csv` has headers but no rows because all
1,301 expected runs completed successfully.
