# Real Sponza/Bistro material-bin sweep plots

- Successful runs: 28
- Camera-path samples per run: 42
- Renderers: Deferred + prepass (variant 8), Visibility + G-buffer (variant 9)
- Main x-axis: realized active material-bin count, with requested max-open and diversity in labels.

## Figures

- `00_sweep_overview`: absolute total time and paired renderer ratio.
- `01_total_time_sweep`: mean total time with camera-path P10–P90 bands.
- `02_paired_renderer_ratio`: frame-matched visibility/deferred ratio.
- `03_normalized_to_one_bin`: renderer sensitivity relative to the 1-bin baseline.
- `04_visibility_pass_breakdown`: visibility renderer pass composition.
- `05_camera_path_ratio_heatmap`: ratio across camera path and sweep conditions.
- `06_matched_diversity_contrast`: diversity 1 versus 0 at equal active-bin counts.
- `07_bin_cap_realization`: requested cap versus actual bins.

## Notes

P10–P90 ranges describe variation along the camera path, not repeated-run confidence intervals. Each condition was run once.