# Sponza raster-metric comparison

Hardware: **NVIDIA GeForce RTX 5060 Ti 16GB**

Each scene folder contains one two-panel plot per raster metric:

- top: Deferred total, Prepass total, VisBuf total, and the metric;
- bottom: every renderer pass, with the same metric repeated on the right axis.

All curves use the same playback camera, 1920×1080, VFC enabled, 60 warm-up
frames, 2500 measured frames, and 10-frame mean
windows. The raster renderer uses the same Donut Assimp scene hierarchy
converted to benchmark buffers, so its VFC workload matches the timed Donut
renderers exactly.

Workload validation:

- sponza: 250 timing windows, 2500 raster frames, max |index difference| = 0.000000
- sponza_ivy: 250 timing windows, 2500 raster frames, max |index difference| = 0.000000

Metrics:

- `triangle_count`: Visible triangle count (triangles)
- `total_fragments`: Total fragments (fragments)
- `covered_pixels`: Covered pixels (pixels)
- `overdraw_extra`: Extra overdraw fragments (fragments)
- `avg_overdraw`: Average overdraw (fragments / covered pixel)
- `max_overdraw`: Maximum overdraw (fragments / pixel)
- `rasterized_triangles`: Rasterized triangle count (triangles)
- `skipped_triangles`: Skipped triangle count (triangles)
- `quad_instances`: Quad instances (quads)
- `quad_covered_lanes`: Quad covered lanes (lanes)
- `quad_waste_lanes`: Quad waste lanes (lanes)
- `quad_efficiency`: Quad efficiency (percent)

`correlations.csv` contains Pearson correlations for every total and pass.
`workload_validation.csv` records the frame/index alignment check.

Important: raster metrics come from the project's compute-based software
raster-stat pass. They describe the benchmark raster model and are not native
hardware performance counters.
